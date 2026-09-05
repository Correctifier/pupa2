#include "simulated_pickup.hpp"

#include <cmath>
#include <complex>

namespace pickup::bsp::pc {

ImpedanceSample SimulatedPickup::measure(double frequency_hz) {
  constexpr double pi = 3.14159265358979323846;
  const double omega = 2.0 * pi * frequency_hz;
  const std::complex<double> series(parameters_.dcr_ohm,
                                    omega * parameters_.inductance_h);
  const double capacitance_f = parameters_.capacitance_pf * 1e-12;
  const std::complex<double> admittance = 1.0 / series +
                                          std::complex<double>(0, omega * capacitance_f);
  auto impedance = 1.0 / admittance;
  const double sigma = std::abs(impedance) * parameters_.noise_percent / 100.0;
  std::normal_distribution<double> noise(0.0, sigma);
  impedance += std::complex<double>(noise(generator_), noise(generator_));
  return {frequency_hz, impedance.real(), impedance.imag()};
}

}  // namespace pickup::bsp::pc
