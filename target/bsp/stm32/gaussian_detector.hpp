#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>

#include "interfaces/impedance_analyzer.hpp"

namespace pickup::bsp::stm32 {

class GaussianDetector {
 public:
  static constexpr std::size_t decimation = 4;
  static constexpr float capture_cycles = 5.0F;

  void configure(float frequency_hz, float raw_sample_rate_hz);
  void begin();
  bool process(std::span<const std::uint32_t> input);
  bool result(DemodulatedSignals& output) const;

 private:
  float frequency_hz_{};
  float raw_sample_rate_hz_{};
  std::size_t sample_count_{};
  std::size_t target_count_{};
  std::size_t accumulated_{};
  std::uint32_t voltage_sum_{};
  std::uint32_t sense_sum_{};
  std::complex<float> oscillator_{1.0F, 0.0F};
  std::complex<float> oscillator_step_{1.0F, 0.0F};
  std::complex<float> voltage_result_{};
  std::complex<float> sense_result_{};
  float weight_{};
  float weight_ratio_{};
  float weight_ratio_step_{};
  float weight_sum_{};
  std::uint16_t voltage_min_{4095};
  std::uint16_t voltage_max_{};
  std::uint16_t sense_min_{4095};
  std::uint16_t sense_max_{};
  bool complete_{};
};

}  // namespace pickup::bsp::stm32
