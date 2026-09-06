#pragma once
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace pickup {

struct ProcessedMeasurement {
  float frequency_hz{};
  std::uint32_t range_index{};
  float sense_resistor_ohm{};
  std::complex<float> v{};
  std::complex<float> vsense{};
  std::uint16_t v_min{4095};
  std::uint16_t v_max{};
  std::uint16_t vsense_min{4095};
  std::uint16_t vsense_max{};
  std::complex<float> impedance{};
};

class FourthOrderMovingAverage {
 public:
  static constexpr std::size_t window_length = 32;
  static constexpr std::size_t order = 4;
  static constexpr std::size_t impulse_response_length = order * (window_length - 1) + 1;
  // A rounded full filter window, in ADC frames (one sample per channel).
  static constexpr std::size_t settling_frames = order * window_length;

  static float settling_time_seconds(float sample_rate_hz) {
    return static_cast<float>(settling_frames) / sample_rate_hz;
  }

  std::complex<float> process(std::complex<float> input);
  void reset();

 private:
  struct Stage {
    std::array<std::complex<float>, window_length> values{};
    std::complex<float> sum{};
    std::size_t next_index{};
    std::size_t count{};
  };

  std::array<Stage, order> stages_{};
};

class AcquisitionProcessor {
 public:
  void begin(
      float frequency_hz,
      float sample_rate_hz,
      float adc_full_scale_v = 3.3F
  );
  void process(const std::uint16_t* interleaved, std::size_t count);
  std::optional<ProcessedMeasurement> finish(
      std::uint32_t range_index,
      float sense_resistor_ohm
  ) const;

 private:
  float frequency_hz_{};
  float sample_rate_hz_{};
  float adc_scale_{};
  std::size_t sample_index_{};
  FourthOrderMovingAverage v_filter_;
  FourthOrderMovingAverage vsense_filter_;
  std::complex<float> v_result_{};
  std::complex<float> vsense_result_{};
  std::uint16_t v_min_{4095}, v_max_{};
  std::uint16_t vsense_min_{4095}, vsense_max_{};
};

}  // namespace pickup
