#include "signal_processing.hpp"

#include <algorithm>
#include <cmath>

namespace pickup {

void FourthOrderIntegrator::add(std::complex<float> input) {
  for (auto& sum : sums_) {
    sum += input;
    input = sum;
  }
}

std::complex<float> FourthOrderIntegrator::result() const {
  return sums_.back();
}

void FourthOrderIntegrator::reset() {
  sums_ = {};
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

  v_integrator_.reset();
  vsense_integrator_.reset();

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

    v_integrator_.add(2.0F * v * oscillator);
    vsense_integrator_.add(2.0F * sense * oscillator);
  }
}

std::optional<ProcessedMeasurement> AcquisitionProcessor::finish(
    std::uint32_t range,
    float rsense
) const {
  if (sample_index_ == 0) {
    return std::nullopt;
  }

  // Four cascaded sums have constant-input gain C(N + 3, 4).
  // Normalize once at acquisition end to retain voltage amplitude units.
  const auto count = static_cast<float>(sample_index_);
  const float gain = count * (count + 1.0F) * (count + 2.0F) * (count + 3.0F) / 24.0F;
  const auto v = v_integrator_.result() / gain;
  const auto vsense = vsense_integrator_.result() / gain;

  if (std::abs(vsense) < 1e-15F) {
    return std::nullopt;
  }

  ProcessedMeasurement
      result{
          frequency_hz_,
          range,
          rsense,
          v,
          vsense,
          v_min_,
          v_max_,
          vsense_min_,
          vsense_max_
      };
  result.impedance = rsense * v / vsense;

  return result;
}

}  // namespace pickup
