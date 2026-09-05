#include "simulated_pickup.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pickup::bsp::pc {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double ranges[] = {100.0, 1000.0, 10000.0, 100000.0};
}  // namespace

void SimulatedPickup::set_control(float frequency_hz, float amplitude_v) {
  generator_frequency_hz_ = frequency_hz;
  generator_amplitude_v_ = amplitude_v;
  // Coherent virtual sampling: 64 ADC frames per generated period. A hardware
  // timer implementation should similarly report its actual quantized rate.
  sample_rate_hz_ = frequency_hz * 64.0F;
}

float SimulatedPickup::sense_resistor_ohm() const {
  return static_cast<float>(ranges[range_index_]);
}

bool SimulatedPickup::set_range_manual(std::uint32_t range_index) {
  if (range_index >= std::size(ranges)) {
    return false;
  }
  automatic_range_ = false;
  range_index_ = range_index;
  return true;
}

bool SimulatedPickup::start_acquisition(std::uint16_t* buffer, std::size_t count) {
  if (acquisition_active_) {
    return false;
  }
  if (buffer == nullptr || count < 2 || count % 2 != 0) {
    return false;
  }

  const double omega = 2.0 * pi * generator_frequency_hz_;
  const std::complex<double> series(parameters_.dcr_ohm,
                                    omega * parameters_.inductance_h);
  const auto impedance =
      1.0 / (1.0 / series +
             std::complex<double>(0, omega * parameters_.capacitance_pf * 1e-12));
  if (automatic_range_) {
    double best_error = std::numeric_limits<double>::max();
    for (std::uint32_t index = 0; index < std::size(ranges); ++index) {
      const double error = std::abs(std::log(ranges[index] / std::abs(impedance)));
      if (error < best_error) {
        best_error = error;
        range_index_ = index;
      }
    }
  }

  const std::complex<double> source(generator_amplitude_v_, 0);
  v_signal_ = source * impedance / (impedance + ranges[range_index_]);
  vsense_signal_ = source * ranges[range_index_] / (impedance + ranges[range_index_]);
  dma_buffer_ = buffer;
  dma_buffer_count_ = count;
  clean_count_ = 0;
  acquisition_active_ = true;
  return true;
}

void SimulatedPickup::advance_dma() const {
  if (!acquisition_active_ || clean_count_ == dma_buffer_count_) {
    return;
  }

  const std::size_t end = std::min(dma_buffer_count_, clean_count_ + 256);
  std::normal_distribution<double> noise(0.0, parameters_.noise_percent / 100.0);
  constexpr double adc_counts_per_volt = 4095.0 / 3.3;
  const auto quantize = [](double value) {
    const long counts = std::lround(2048.0 + value * adc_counts_per_volt);
    return static_cast<std::uint16_t>(std::clamp(counts, 0L, 4095L));
  };

  for (std::size_t index = clean_count_; index < end; index += 2) {
    const std::size_t sample = index / 2;
    const double phase = 2.0 * pi * generator_frequency_hz_ * sample / sample_rate_hz_;
    const std::complex<double> carrier(std::cos(phase), std::sin(phase));
    const double v =
        std::real(v_signal_ * carrier) + std::abs(v_signal_) * noise(generator_);
    const double sense = std::real(vsense_signal_ * carrier) +
                         std::abs(vsense_signal_) * noise(generator_);
    dma_buffer_[index] = quantize(v);
    dma_buffer_[index + 1] = quantize(sense);
  }

  clean_count_ = end;
  if (clean_count_ == dma_buffer_count_) {
    acquisition_active_ = false;
  }
}

std::size_t SimulatedPickup::clean_data_count() const {
  advance_dma();
  return clean_count_;
}

bool SimulatedPickup::acquisition_finished() const {
  advance_dma();
  return dma_buffer_ != nullptr && clean_count_ == dma_buffer_count_;
}

}  // namespace pickup::bsp::pc
