#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pickup::bsp {

struct ReceivedLine {
  std::uint32_t endpoint{};
  std::string text;
};

class Transport {
 public:
  virtual ~Transport() = default;
  virtual std::optional<ReceivedLine> receive() = 0;
  virtual void send(std::uint32_t endpoint, std::string_view line) = 0;
};

}  // namespace pickup::bsp

