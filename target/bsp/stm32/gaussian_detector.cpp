#include "gaussian_detector.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float pi = 3.14159265358979323846F;
constexpr float adc_scale = 3.3F / 4095.0F;
}  // namespace

namespace pickup::bsp::stm32 {

void GaussianDetector::configure(float frequency_hz, float raw_sample_rate_hz) {
  frequency_hz_ = frequency_hz;
  raw_sample_rate_hz_ = raw_sample_rate_hz;
  const float effective_rate = raw_sample_rate_hz / static_cast<float>(decimation);
  target_count_ = std::max<std::size_t>(
      1,
      static_cast<std::size_t>(std::ceil(capture_cycles * effective_rate / frequency_hz))
  );
  const float phase_step = -2.0F * pi * frequency_hz / effective_rate;
  oscillator_step_ = {
      std::cos(phase_step),
      std::sin(phase_step),
  };
}

void GaussianDetector::begin() {
  sample_count_ = 0;
  accumulated_ = 0;
  voltage_sum_ = 0;
  sense_sum_ = 0;
  oscillator_ = {1.0F, 0.0F};
  voltage_result_ = {};
  sense_result_ = {};
  weight_sum_ = 0.0F;
  voltage_min_ = 4095;
  voltage_max_ = 0;
  sense_min_ = 4095;
  sense_max_ = 0;
  complete_ = false;

  const float center = static_cast<float>(target_count_ - 1) * 0.5F;
  const float sigma = std::max(1.0F, static_cast<float>(target_count_) / 10.0F);
  const float sigma_squared = sigma * sigma;
  weight_ = std::exp(-0.5F * center * center / sigma_squared);
  weight_ratio_ = std::exp((2.0F * center - 1.0F) / (2.0F * sigma_squared));
  weight_ratio_step_ = std::exp(-1.0F / sigma_squared);
}

bool GaussianDetector::process(std::span<const std::uint32_t> input) {
  for (const auto pair : input) {
    if (complete_) {
      break;
    }

    voltage_sum_ += pair & 4095U;
    sense_sum_ += (pair >> 16) & 4095U;

    if (++accumulated_ != decimation) {
      continue;
    }

    const auto voltage = static_cast<std::uint16_t>((voltage_sum_ + decimation / 2) / decimation);
    const auto sense = static_cast<std::uint16_t>((sense_sum_ + decimation / 2) / decimation);
    const float centered_voltage = static_cast<float>(voltage) - 2048.0F;
    const float centered_sense = static_cast<float>(sense) - 2048.0F;

    voltage_min_ = std::min(voltage_min_, voltage);
    voltage_max_ = std::max(voltage_max_, voltage);
    sense_min_ = std::min(sense_min_, sense);
    sense_max_ = std::max(sense_max_, sense);
    voltage_result_ += weight_ * centered_voltage * oscillator_;
    sense_result_ += weight_ * centered_sense * oscillator_;
    weight_sum_ += weight_;

    oscillator_ *= oscillator_step_;
    weight_ *= weight_ratio_;
    weight_ratio_ *= weight_ratio_step_;
    voltage_sum_ = 0;
    sense_sum_ = 0;
    accumulated_ = 0;
    complete_ = ++sample_count_ == target_count_;
  }

  const float oscillator_magnitude = std::abs(oscillator_);

  if (oscillator_magnitude > 0.0F) {
    oscillator_ /= oscillator_magnitude;
  }

  return complete_;
}

bool GaussianDetector::result(DemodulatedSignals& output) const {
  if (!complete_ || weight_sum_ <= 0.0F) {
    return false;
  }

  const float angle = pi * frequency_hz_ / raw_sample_rate_hz_;
  const float boxcar_gain = std::sin(decimation * angle) / (decimation * std::sin(angle));
  const float scale = 2.0F * adc_scale / (weight_sum_ * boxcar_gain);
  output = {
      scale * voltage_result_,
      scale * sense_result_,
      voltage_min_,
      voltage_max_,
      sense_min_,
      sense_max_,
  };

  return true;
}

}  // namespace pickup::bsp::stm32
