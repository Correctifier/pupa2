#include "simulated_pickup.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pickup::bsp::pc {

SimulatedPickup::SimulatedPickup(Clock clock) : clock_(clock), circuit_time_(clock_()) {}

double SimulatedPickup::steady_seconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::uint32_t SimulatedPickup::milliseconds() const {
  return static_cast<std::uint32_t>(static_cast<std::uint64_t>(clock_() * 1000.0));
}

void SimulatedPickup::set_parameters(PickupParameters parameters) {
  if (!std::isfinite(parameters.noise_percent) || parameters.noise_percent < 0) {
    throw std::invalid_argument("noise must be nonnegative and finite");
  }

  advance_to_now();
  circuit_.set_parameters(parameters);

  parameters_ = parameters;
}

SimulatorStatus SimulatedPickup::status() const {
  advance_to_now();

  return {
      generator_frequency_hz_,
      generator_amplitude_v_,
      range_index_,
      sense_resistor_ohm(),
      automatic_range_,
  };
}

void SimulatedPickup::set_control(float frequency_hz, float amplitude_v) {
  advance_to_now();
  circuit_.set_generator(frequency_hz, amplitude_v);

  // Changing generator settings during DMA invalidates this measurement.
  acquisition_invalid_ = acquisition_invalid_ || acquisition_active_;
  generator_frequency_hz_ = frequency_hz;
  generator_amplitude_v_ = amplitude_v;
  sample_rate_hz_ = frequency_hz * 64.0F;

  if (automatic_range_) {
    range_index_ = next_range_index_;

    circuit_.set_sense_resistor(sense_resistor_ohm());
  }
}

float SimulatedPickup::sense_resistor_ohm() const {
  return sense_resistors_ohm[range_index_];
}

void SimulatedPickup::set_range_auto() {
  advance_to_now();

  automatic_range_ = true;
  next_range_index_ = range_index_;
}

bool SimulatedPickup::set_range_manual(std::uint32_t range_index) {
  if (range_index >= sense_resistors_ohm.size()) {
    return false;
  }

  advance_to_now();

  acquisition_invalid_ =
      acquisition_invalid_ || (acquisition_active_ && range_index != range_index_);
  automatic_range_ = false;
  range_index_ = range_index;
  next_range_index_ = range_index;

  circuit_.set_sense_resistor(sense_resistor_ohm());

  return true;
}

bool SimulatedPickup::start_acquisition(std::uint16_t* buffer, std::size_t count) {
  advance_to_now();

  if (acquisition_active_ || buffer == nullptr || count < 2 || count % 2 != 0) {
    return false;
  }

  dma_buffer_ = buffer;
  dma_buffer_count_ = count;
  clean_count_ = 0;
  acquisition_active_ = true;
  acquisition_invalid_ = false;
  voltage_power_ = 0;
  sense_power_ = 0;
  acquisition_sample_interval_ = 1.0 / sample_rate_hz_;
  next_sample_time_ = circuit_time_ + acquisition_sample_interval_;

  return true;
}

void SimulatedPickup::advance_to_now() const {
  const double now = clock_();
  constexpr double adc_counts_per_volt = 4095.0 / 3.3;
  // Input-referred noise is proportional to generator peak amplitude, independent
  // of instantaneous waveform phase, and does not alter the circuit state.
  std::normal_distribution<double> noise(0.0, 1.0);
  const double noise_scale = generator_amplitude_v_ * parameters_.noise_percent / 100.0;
  const auto quantize = [](double value) {
    const long counts = std::lround(2048.0 + value * adc_counts_per_volt);

    return static_cast<std::uint16_t>(std::clamp(
        counts,
        0L,
        4095L
    ));
  };

  while (acquisition_active_ && next_sample_time_ <= now) {
    const auto sample = circuit_.advance(std::max(0.0, next_sample_time_ - circuit_time_));
    circuit_time_ = next_sample_time_;
    const double v = sample.source_v + noise_scale * noise(generator_);
    const double sense = sample.vsense + noise_scale * noise(generator_);
    dma_buffer_[clean_count_] = acquisition_invalid_ ? 2048 : quantize(v);
    dma_buffer_[clean_count_ + 1] = acquisition_invalid_ ? 2048 : quantize(sense);
    const double measured_exciter = static_cast<double>(dma_buffer_[clean_count_]) - 2048.0;
    const double measured_sense = static_cast<double>(dma_buffer_[clean_count_ + 1]) - 2048.0;
    const double measured_dut = measured_exciter - measured_sense;
    voltage_power_ += measured_dut * measured_dut;
    sense_power_ += measured_sense * measured_sense;
    clean_count_ += 2;
    next_sample_time_ += acquisition_sample_interval_;

    if (clean_count_ == dma_buffer_count_) {
      acquisition_active_ = false;

      if (acquisition_invalid_) {
        std::fill_n(
            dma_buffer_,
            dma_buffer_count_,
            2048
        );
      }

      update_next_range();
    }
  }

  // Evolve idle/settling intervals as well, without consuming CPU per ADC sample.
  circuit_.advance(std::max(0.0, now - circuit_time_));

  circuit_time_ = std::max(now, circuit_time_);
}

void SimulatedPickup::update_next_range() const {
  if (!automatic_range_ || acquisition_invalid_ || voltage_power_ <= 0 || sense_power_ <= 0) {
    return;
  }

  const double impedance = sense_resistor_ohm() * std::sqrt(voltage_power_ / sense_power_);
  double best_error = std::numeric_limits<double>::infinity();

  for (std::uint32_t index = 0; index < sense_resistors_ohm.size(); ++index) {
    const double error = std::abs(std::log(sense_resistors_ohm[index] / impedance));

    if (error < best_error) {
      best_error = error;
      next_range_index_ = index;
    }
  }
}

std::size_t SimulatedPickup::clean_data_count() const {
  advance_to_now();

  return acquisition_active_ ? 0 : clean_count_;
}

bool SimulatedPickup::acquisition_finished() const {
  advance_to_now();

  return dma_buffer_ != nullptr && !acquisition_active_;
}

}  // namespace pickup::bsp::pc
