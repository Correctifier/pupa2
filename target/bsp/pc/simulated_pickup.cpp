#include "simulated_pickup.hpp"

#include <cmath>
#include <complex>
#include <limits>
#include <utility>

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
  constexpr double ranges[] = {100.0, 1000.0, 10000.0, 100000.0};
  if (automatic_range_) {
    double best_error = std::numeric_limits<double>::max();
    for (std::uint32_t index = 0; index < 4; ++index) {
      const double error = std::abs(std::log(ranges[index] / std::abs(impedance)));
      if (error < best_error) { best_error = error; range_index_ = index; }
    }
  }
  const double rsense = ranges[range_index_];
  const std::complex<double> source(generator_amplitude_v_, 0.0);
  const auto v = source * impedance / (impedance + rsense);
  const auto vsense = source * rsense / (impedance + rsense);
  const auto adc_limits = [](double magnitude) {
    const int excursion = static_cast<int>(std::round(std::min(1.0, magnitude / 1.65) * 2047.0));
    return std::pair<std::uint16_t, std::uint16_t>(2048 - excursion, 2048 + excursion);
  };
  const auto [v_min, v_max] = adc_limits(std::abs(v));
  const auto [vs_min, vs_max] = adc_limits(std::abs(vsense));
  return {frequency_hz, range_index_, rsense, v.real(), v.imag(), vsense.real(),
          vsense.imag(), v_min, v_max, vs_min, vs_max, impedance.real(), impedance.imag()};
}

void SimulatedPickup::set_generator(double frequency_hz, double amplitude_v) {
  generator_frequency_hz_ = frequency_hz;
  generator_amplitude_v_ = amplitude_v;
}

bool SimulatedPickup::set_range_manual(std::uint32_t range_index) {
  if (range_index >= 4) return false;
  automatic_range_ = false;
  range_index_ = range_index;
  return true;
}

}  // namespace pickup::bsp::pc
