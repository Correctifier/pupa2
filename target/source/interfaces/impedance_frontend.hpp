#pragma once

namespace pickup::bsp {

struct ImpedanceSample {
  double frequency_hz{};
  double real_ohm{};
  double imaginary_ohm{};
};

class ImpedanceFrontend {
 public:
  virtual ~ImpedanceFrontend() = default;
  virtual ImpedanceSample measure(double frequency_hz) = 0;
};

}  // namespace pickup::bsp

