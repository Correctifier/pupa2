#pragma once
#include <cstdint>

namespace pickup::protocol {

struct SweepStartRequest {
  std::uint32_t endpoint{};
  float start_hz{};
  float stop_hz{};
  std::uint32_t points{};
};

struct SweepStopRequest {};

}  // namespace pickup::protocol
