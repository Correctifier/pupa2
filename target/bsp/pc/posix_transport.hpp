#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

#include "interfaces/transport.hpp"

namespace pickup::bsp::pc {

class PosixTransport final : public Transport {
 public:
  explicit PosixTransport(std::uint16_t tcp_port);
  ~PosixTransport() override;
  PosixTransport(const PosixTransport&) = delete;
  PosixTransport& operator=(const PosixTransport&) = delete;

  std::optional<ReceivedLine> receive() override;
  void send(std::uint32_t endpoint, std::string_view line) override;
  const std::string& serial_path() const {
    return serial_path_;
  }

 private:
  void accept_clients();
  void read_endpoints();
  void close_endpoint(std::uint32_t endpoint);

  int server_fd_{-1};
  int pty_master_fd_{-1};
  std::uint32_t next_endpoint_{2};
  std::string serial_path_;
  std::unordered_map<std::uint32_t, int> endpoints_;
  std::unordered_map<std::uint32_t, std::string> buffers_;
  std::deque<ReceivedLine> received_;
};

}  // namespace pickup::bsp::pc
