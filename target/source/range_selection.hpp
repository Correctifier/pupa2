#pragma once

#include <array>
#include <cstdint>

namespace pickup {

// Protocol range indices and the two-bit hardware selector use the same encoding.
enum class RangeSelection : std::uint32_t {
  ohm_1k = 0,
  ohm_10k = 1,
  ohm_100k = 2,
  ohm_1m = 3,
};

inline constexpr std::array<float, 4> sense_resistors_ohm{
    1000.0F,
    10000.0F,
    100000.0F,
    1000000.0F,
};
inline constexpr auto startup_range = RangeSelection::ohm_100k;
inline constexpr auto startup_range_index = static_cast<std::uint32_t>(startup_range);

}  // namespace pickup
