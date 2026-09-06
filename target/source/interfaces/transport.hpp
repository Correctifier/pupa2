#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace pickup::bsp {

struct ReceivedLine {
  static constexpr std::size_t capacity = 1024;
  std::uint32_t endpoint{};
  std::array<char, capacity> text{};
  std::size_t size{};

  std::string_view view() const {
    return {text.data(), size};
  }
};

class Transport {
 public:
  virtual ~Transport() = default;

  virtual std::optional<ReceivedLine> receive() = 0;
  virtual void send(std::uint32_t endpoint, std::string_view line) = 0;
};

}  // namespace pickup::bsp
