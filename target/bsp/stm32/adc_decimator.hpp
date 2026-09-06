#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "signal_processing.hpp"

namespace pickup::bsp::stm32 {
// Boxcar averaging of simultaneous ADC pairs, retaining partial groups across DMA halves.
class AdcDecimator {
 public:
  void configure(float frequency_hz, float raw_sample_rate_hz) {
    const float samples_per_cycle = static_cast<float>(FourthOrderMovingAverage::window_length);
    divisor_ = std::max<std::uint32_t>(
        1U,
        static_cast<std::uint32_t>(raw_sample_rate_hz / (samples_per_cycle * frequency_hz))
    );
    sample_rate_ = raw_sample_rate_hz / static_cast<float>(divisor_);
  }

  void begin(std::span<std::uint16_t> output) {
    output_ = output;
    written_ = 0;
    accumulated_ = 0;
    voltage_sum_ = 0;
    sense_sum_ = 0;
  }

  bool process(std::span<const std::uint32_t> input) {
    for (const auto pair : input) {
      if (written_ == output_.size()) {
        return true;
      }

      voltage_sum_ += pair & 4095U;
      sense_sum_ += (pair >> 16) & 4095U;

      if (++accumulated_ == divisor_) {
        output_[written_++] = static_cast<std::uint16_t>((voltage_sum_ + divisor_ / 2) / divisor_);
        output_[written_++] = static_cast<std::uint16_t>((sense_sum_ + divisor_ / 2) / divisor_);
        accumulated_ = 0;
        voltage_sum_ = 0;
        sense_sum_ = 0;
      }
    }

    return written_ == output_.size();
  }

  float sample_rate_hz() const {
    return sample_rate_;
  }

 private:
  std::span<std::uint16_t> output_;
  std::size_t written_{};
  std::uint32_t divisor_{1};
  std::uint32_t accumulated_{};
  std::uint32_t voltage_sum_{};
  std::uint32_t sense_sum_{};
  float sample_rate_{};
};
}  // namespace pickup::bsp::stm32
