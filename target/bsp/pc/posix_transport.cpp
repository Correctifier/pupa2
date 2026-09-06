#include "posix_transport.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace pickup::bsp::pc {
namespace {
void make_nonblocking(int fd) {
  const int flags = fcntl(
      fd,
      F_GETFL,
      0
  );

  if (flags < 0 || fcntl(
      fd,
      F_SETFL,
      flags | O_NONBLOCK
  ) < 0) {
    throw std::runtime_error("failed to set nonblocking mode");
  }
}
}  // namespace

PosixTransport::PosixTransport(std::uint16_t tcp_port) {
  server_fd_ = socket(
      AF_INET,
      SOCK_STREAM,
      0
  );

  if (server_fd_ < 0) {
    throw std::runtime_error("could not create TCP socket");
  }

  int reuse = 1;

  setsockopt(
      server_fd_,
      SOL_SOCKET,
      SO_REUSEADDR,
      &reuse,
      sizeof(reuse)
  );

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(tcp_port);

  if (bind(
      server_fd_,
      reinterpret_cast<sockaddr*>(&address),
      sizeof(address)
  ) < 0 ||
      listen(server_fd_, 4) < 0) {
    throw std::runtime_error(std::string("could not listen: ") + strerror(errno));
  }

  make_nonblocking(server_fd_);

  pty_master_fd_ = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);

  if (pty_master_fd_ >= 0 && grantpt(pty_master_fd_) == 0 && unlockpt(pty_master_fd_) == 0) {
    if (const char* path = ptsname(pty_master_fd_)) {
      serial_path_ = path;
    }

    endpoints_[1] = pty_master_fd_;
  }
}

PosixTransport::~PosixTransport() {
  for (const auto& [_, fd] : endpoints_) {
    close(fd);
  }

  if (server_fd_ >= 0) {
    close(server_fd_);
  }
}

void PosixTransport::accept_clients() {
  while (true) {
    const int fd = accept(
        server_fd_,
        nullptr,
        nullptr
    );

    if (fd < 0) {
      break;
    }

    // Acknowledgements and measurement events are separate small messages.
    // Send both promptly instead of delaying each adaptive point behind Nagle.
    int no_delay = 1;

    setsockopt(
        fd,
        IPPROTO_TCP,
        TCP_NODELAY,
        &no_delay,
        sizeof(no_delay)
    );
    make_nonblocking(fd);

    endpoints_[next_endpoint_++] = fd;
  }
}

void PosixTransport::close_endpoint(std::uint32_t endpoint) {
  if (endpoint == 1) {
    return;
  }

  close(endpoints_.at(endpoint));
  endpoints_.erase(endpoint);
  buffers_.erase(endpoint);
}

void PosixTransport::read_endpoints() {
  std::vector<std::uint32_t> closed;
  char chunk[1024];

  for (const auto& [endpoint, fd] : endpoints_) {
    while (true) {
      const auto count = read(
          fd,
          chunk,
          sizeof(chunk)
      );

      if (count > 0) {
        auto& buffer = buffers_[endpoint];

        buffer.append(chunk, static_cast<std::size_t>(count));

        auto newline = buffer.find('\n');

        while (newline != std::string::npos) {
          ReceivedLine line;
          line.endpoint = endpoint;
          line.size = std::min(newline, ReceivedLine::capacity);

          std::copy_n(
              buffer.data(),
              line.size,
              line.text.data()
          );
          received_.push_back(line);
          buffer.erase(0, newline + 1);

          newline = buffer.find('\n');
        }
      } else {
        if (count == 0 && endpoint != 1) {
          closed.push_back(endpoint);
        }

        break;
      }
    }
  }

  for (auto endpoint : closed) {
    close_endpoint(endpoint);
  }
}

std::optional<ReceivedLine> PosixTransport::receive() {
  accept_clients();
  read_endpoints();

  if (received_.empty()) {
    return std::nullopt;
  }

  auto line = std::move(received_.front());

  received_.pop_front();

  return line;
}

void PosixTransport::send(std::uint32_t endpoint, std::string_view line) {
  const auto found = endpoints_.find(endpoint);

  if (found == endpoints_.end()) {
    return;
  }

  const char* data = line.data();
  std::size_t remaining = line.size();

  while (remaining > 0) {
    const auto sent = write(
        found->second,
        data,
        remaining
    );

    if (sent > 0) {
      data += sent;
      remaining -= static_cast<std::size_t>(sent);
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
      close_endpoint(endpoint);

      return;
    } else {
      return;
    }
  }
}

}  // namespace pickup::bsp::pc
