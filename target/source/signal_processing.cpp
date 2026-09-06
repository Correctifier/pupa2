#include "signal_processing.hpp"

#include <algorithm>
#include <cmath>

namespace pickup {

void FourthOrderMovingAverage::reset() {
  stages_ = {};
}

std::complex<float> FourthOrderMovingAverage::process(std::complex<float> input) {
  for (auto& stage : stages_) {
    if (stage.count == window_size_) {
      stage.sum -= stage.values[stage.next_index];
    } else {
      ++stage.count;
    }

    stage.values[stage.next_index] = input;
    stage.sum += input;
    stage.next_index = (stage.next_index + 1) % window_size_;
    input = stage.sum / static_cast<float>(stage.count);
  }
  return input;
}

void AcquisitionProcessor::begin(
    float frequency_hz,
    float sample_rate_hz,
    float adc_full_scale_v
) {
  frequency_hz_ = frequency_hz;
  sample_rate_hz_ = sample_rate_hz;
  adc_scale_ = adc_full_scale_v / 4095.0F;
  sample_index_ = 0;
  settled_outputs_ = 0;
  v_filter_.reset();
  vsense_filter_.reset();
  v_result_ = {};
  vsense_result_ = {};
  v_min_ = 4095;
  v_max_ = 0;
  vsense_min_ = 4095;
  vsense_max_ = 0;
}

void AcquisitionProcessor::process(const std::uint16_t* data, std::size_t count) {
  constexpr float pi = 3.14159265358979323846F;
  for (std::size_t i = 0; i + 1 < count; i += 2, ++sample_index_) {
    const auto raw_v = data[i];
    const auto raw_sense = data[i + 1];
    v_min_ = std::min(v_min_, raw_v);
    v_max_ = std::max(v_max_, raw_v);
    vsense_min_ = std::min(vsense_min_, raw_sense);
    vsense_max_ = std::max(vsense_max_, raw_sense);

    const float phase =
        -2.0F * pi * frequency_hz_ * static_cast<float>(sample_index_) / sample_rate_hz_;
    const std::complex<float> oscillator(std::cos(phase), std::sin(phase));
    const float v = (static_cast<float>(raw_v) - 2048.0F) * adc_scale_;
    const float sense = (static_cast<float>(raw_sense) - 2048.0F) * adc_scale_;
    v_result_ = v_filter_.process(2.0F * v * oscillator);
    vsense_result_ = vsense_filter_.process(2.0F * sense * oscillator);
    ++settled_outputs_;
  }
}

std::optional<ProcessedMeasurement> AcquisitionProcessor::finish(
    std::uint32_t range,
    float rsense
) const {
  if (sample_index_ == 0 || std::abs(vsense_result_) < 1e-15F) {
    return std::nullopt;
  }

  ProcessedMeasurement result{
      frequency_hz_,
      range,
      rsense,
      v_result_,
      vsense_result_,
      v_min_,
      v_max_,
      vsense_min_,
      vsense_max_
  };
  result.impedance = rsense * v_result_ / vsense_result_;
  return result;
}

}  // namespace pickup
