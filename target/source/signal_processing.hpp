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
  std::complex<float> process(std::complex<float> input);
  void reset();

 private:
  static constexpr std::size_t window_size_ = 32;

  struct Stage {
    std::array<std::complex<float>, window_size_> values{};
    std::complex<float> sum{};
    std::size_t next_index{};
    std::size_t count{};
  };

  std::array<Stage, 4> stages_;
};

class AcquisitionProcessor {
 public:
  void begin(float frequency_hz, float sample_rate_hz, float adc_full_scale_v = 3.3F);
  void process(const std::uint16_t* interleaved, std::size_t count);
  std::optional<ProcessedMeasurement> finish(std::uint32_t range_index,
                                             float sense_resistor_ohm) const;

 private:
  float frequency_hz_{};
  float sample_rate_hz_{};
  float adc_scale_{};
  std::size_t sample_index_{};
  std::size_t settled_outputs_{};
  FourthOrderMovingAverage v_filter_;
  FourthOrderMovingAverage vsense_filter_;
  std::complex<float> v_result_{};
  std::complex<float> vsense_result_{};
  std::uint16_t v_min_{4095}, v_max_{};
  std::uint16_t vsense_min_{4095}, vsense_max_{};
};

}  // namespace pickup
