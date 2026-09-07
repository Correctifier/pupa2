#include "frontend.hpp"

#include "acquisition.hpp"
#include "generator.hpp"
#include "hal_support.hpp"
#include "range_selection.hpp"
#include "ranges.hpp"

namespace pickup::bsp::stm32 {
std::uint32_t Frontend::milliseconds() const {
  return HAL_GetTick();
}

bool Frontend::supports_control(float frequency_hz, float amplitude_v) const {
  return generator::supports_control(frequency_hz, amplitude_v);
}

void Frontend::set_control(float frequency_hz, float amplitude_v) {
  if (!supports_control(frequency_hz, amplitude_v)) {
    detail::fail();
  }

  acquisition::invalidate();
  ranges::apply_pending();
  generator::set_control(frequency_hz, amplitude_v);
  acquisition::configure(frequency_hz, generator::sample_rate_hz());
}

bool Frontend::start_acquisition(std::uint16_t* buffer, std::size_t count) {
  return acquisition::start(buffer, count);
}

std::size_t Frontend::clean_data_count() const {
  return acquisition::clean_data_count();
}

bool Frontend::acquisition_finished() const {
  const bool was_active = acquisition::active();
  const bool finished = acquisition::finish();

  if (was_active && finished && acquisition::valid()) {
    DemodulatedSignals signals;

    if (acquisition::result(signals)) {
      ranges::observe(std::norm(signals.v - signals.vsense), std::norm(signals.vsense));
    }
  }

  return finished;
}

bool Frontend::streams_demodulation() const {
  return true;
}

bool Frontend::read_demodulated(DemodulatedSignals& output) const {
  return acquisition::result(output);
}

float Frontend::sample_rate_hz() const {
  return acquisition::sample_rate_hz();
}

void Frontend::set_range_auto() {
  ranges::set_auto();
}

bool Frontend::set_range_manual(std::uint32_t index) {
  if (index >= sense_resistors_ohm.size()) {
    return false;
  }

  if (index != ranges::index()) {
    acquisition::invalidate();
  }

  return ranges::set_manual(index);
}

std::uint32_t Frontend::range_index() const {
  return ranges::index();
}

float Frontend::sense_resistor_ohm() const {
  return ranges::resistance_ohm();
}

void Frontend::calibrate() {
  acquisition::calibrate();
}
}  // namespace pickup::bsp::stm32
