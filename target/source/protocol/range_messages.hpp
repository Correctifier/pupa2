#pragma once
#include <cstdint>

namespace pickup::protocol {

enum class RangeMode { automatic, manual };
struct RangeSetRequest {
  RangeMode mode{RangeMode::automatic};
  std::uint32_t index{};
};

}  // namespace pickup::protocol
