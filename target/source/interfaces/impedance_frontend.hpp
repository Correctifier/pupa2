#pragma once

#include <cstdint>

namespace pickup::bsp {

struct ImpedanceSample {
  double frequency_hz{};
  std::uint32_t range_index{};
  double sense_resistor_ohm{};
  double v_real{};
  double v_imaginary{};
  double vsense_real{};
  double vsense_imaginary{};
  std::uint16_t v_min{};
  std::uint16_t v_max{};
  std::uint16_t vsense_min{};
  std::uint16_t vsense_max{};
  double real_ohm{};
  double imaginary_ohm{};
};

class ImpedanceFrontend {
 public:
  virtual ~ImpedanceFrontend() = default;
  virtual ImpedanceSample measure(double frequency_hz) = 0;
  virtual void set_generator(double frequency_hz, double amplitude_v) = 0;
  virtual void set_range_auto() = 0;
  virtual bool set_range_manual(std::uint32_t range_index) = 0;
  virtual void calibrate() = 0;
};

}  // namespace pickup::bsp
