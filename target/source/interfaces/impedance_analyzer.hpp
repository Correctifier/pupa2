#pragma once
#include <complex>
#include <cstddef>
#include <cstdint>

namespace pickup::bsp {

struct DemodulatedSignals {
  std::complex<float> v{};
  std::complex<float> vsense{};
  std::uint16_t v_min{4095};
  std::uint16_t v_max{};
  std::uint16_t vsense_min{4095};
  std::uint16_t vsense_max{};
};

class ImpedanceAnalyzer {
 public:
  virtual ~ImpedanceAnalyzer() = default;

  // Monotonic milliseconds, wrapping modulo 2^32.
  virtual std::uint32_t milliseconds() const = 0;

  virtual bool supports_control(float, float) const {
    return true;
  }

  virtual void set_control(float frequency_hz, float amplitude_v) = 0;
  // buffer_count and clean_data_count are scalar uint16_t entries. Samples are
  // interleaved [Vdut, Vsense, Vdut, Vsense, ...]. Returns false while busy.
  virtual bool start_acquisition(std::uint16_t* buffer, std::size_t buffer_count) = 0;
  virtual std::size_t clean_data_count() const = 0;
  virtual bool acquisition_finished() const = 0;
  virtual float sample_rate_hz() const = 0;
  virtual bool streams_demodulation() const {
    return false;
  }
  virtual bool read_demodulated(DemodulatedSignals&) const {
    return false;
  }

  virtual void set_range_auto() = 0;
  virtual bool set_range_manual(std::uint32_t range_index) = 0;
  virtual std::uint32_t range_index() const = 0;
  virtual float sense_resistor_ohm() const = 0;
  virtual void calibrate() = 0;
};

}  // namespace pickup::bsp
