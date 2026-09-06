#pragma once
#include <cstdint>

namespace pickup::protocol {

struct GeneratorSetRequest {
  float frequency_hz{};
  float amplitude_v{};
};

}  // namespace pickup::protocol
