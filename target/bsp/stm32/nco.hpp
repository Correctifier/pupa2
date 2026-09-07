#pragma once

#include <cstdint>
#include <span>

namespace pickup::bsp::stm32 {
// Allocation-free oscillator; refill calls continue the same 32-bit phase sequence.
class Nco {
 public:
  void reset(
      float frequency_hz,
      float sample_rate_hz,
      float amplitude_v
  );
  // Change waveform parameters without disturbing the accumulated phase.
  void configure(
      float frequency_hz,
      float sample_rate_hz,
      float amplitude_v
  );
  std::uint16_t next();
  void fill(std::span<std::uint16_t> output);

 private:
  std::uint32_t phase_{};
  std::uint32_t increment_{};
  std::int32_t amplitude_{};
};
}  // namespace pickup::bsp::stm32
