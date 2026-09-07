#include "pickup_circuit.hpp"

#include <cmath>
#include <stdexcept>

#include "range_selection.hpp"

namespace pickup::bsp::pc {
namespace {
constexpr double tau = 6.28318530717958647692;
}

PickupCircuit::PickupCircuit() : sense_resistor_ohm_(sense_resistors_ohm[startup_range_index]) {
  update_coefficients();
}

void PickupCircuit::set_parameters(PickupParameters parameters) {
  if (!std::isfinite(parameters.dcr_ohm) || parameters.dcr_ohm <= 0 ||
      !std::isfinite(parameters.inductance_h) || parameters.inductance_h <= 0 ||
      !std::isfinite(parameters.capacitance_pf) || parameters.capacitance_pf <= 0 ||
      !std::isfinite(parameters.parallel_loss_ohm) || parameters.parallel_loss_ohm <= 0) {
    throw std::invalid_argument("circuit R, L, C and parallel loss must be positive and finite");
  }

  parameters_ = parameters;

  update_coefficients();
}

void PickupCircuit::set_generator(double frequency_hz, double amplitude_v) {
  if (!std::isfinite(frequency_hz) || frequency_hz <= 0 || !std::isfinite(amplitude_v) ||
      amplitude_v < 0) {
    throw std::invalid_argument("source needs positive frequency and nonnegative amplitude");
  }

  frequency_hz_ = frequency_hz;
  amplitude_v_ = amplitude_v;

  update_coefficients();
}

void PickupCircuit::set_sense_resistor(double resistance_ohm) {
  if (!std::isfinite(resistance_ohm) || resistance_ohm <= 0) {
    throw std::invalid_argument("sense resistance must be positive and finite");
  }

  sense_resistor_ohm_ = resistance_ohm;

  update_coefficients();
}

void PickupCircuit::update_coefficients() {
  const double capacitance = parameters_.capacitance_pf * 1e-12;
  a_ = -(1.0 / sense_resistor_ohm_ + 1.0 / parameters_.parallel_loss_ohm) / capacitance;
  b_ = -1.0 / capacitance;
  c_ = 1.0 / parameters_.inductance_h;
  d_ = -parameters_.dcr_ohm / parameters_.inductance_h;
  const std::complex<double> jw(0.0, tau * frequency_hz_);
  const auto branch = parameters_.dcr_ohm + jw * parameters_.inductance_h;
  const auto impedance =
      1.0 / (1.0 / branch + jw * capacitance + 1.0 / parameters_.parallel_loss_ohm);
  forced_voltage_ = amplitude_v_ * impedance / (sense_resistor_ohm_ + impedance);
  forced_current_ = forced_voltage_ / branch;
}

CircuitSample PickupCircuit::advance(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0) {
    throw std::invalid_argument("elapsed circuit time must be nonnegative and finite");
  }

  if (seconds == 0) {
    return sample();
  }

  const std::complex<double> before(std::cos(phase_), std::sin(phase_));
  const double transient_v = voltage_ - std::real(forced_voltage_ * before);
  const double transient_i = current_ - std::real(forced_current_ * before);
  const double mean = (a_ + d_) * 0.5;
  const double half_difference = (a_ - d_) * 0.5;
  const double discriminant = half_difference * half_difference + b_ * c_;
  double diagonal{}, off_diagonal{};

  // exp(A*t) = diagonal*I + off_diagonal*(A - trace(A)/2*I).
  // Resolve real poles separately to avoid exp(mean*t)*cosh(q*t) overflow.
  if (discriminant > 0) {
    const double root = std::sqrt(discriminant);
    const double fast_pole = mean - root;
    const double determinant = a_ * d_ - b_ * c_;
    const double slow_pole = determinant / fast_pole;
    const double slow = std::exp(slow_pole * seconds);
    const double fast = std::exp(fast_pole * seconds);
    diagonal = (slow + fast) * 0.5;
    off_diagonal = slow * -std::expm1(-2.0 * root * seconds) / (2.0 * root);
  } else if (discriminant < 0) {
    const double root = std::sqrt(-discriminant);
    const double decay = std::exp(mean * seconds);
    diagonal = decay * std::cos(root * seconds);
    off_diagonal = decay * std::sin(root * seconds) / root;
  } else {
    diagonal = std::exp(mean * seconds);
    off_diagonal = diagonal * seconds;
  }

  phase_ = std::remainder(phase_ + tau * frequency_hz_ * seconds, tau);
  const std::complex<double> after(std::cos(phase_), std::sin(phase_));
  voltage_ = std::real(forced_voltage_ * after) + diagonal * transient_v +
             off_diagonal * (half_difference * transient_v + b_ * transient_i);
  current_ = std::real(forced_current_ * after) + diagonal * transient_i +
             off_diagonal * (c_ * transient_v - half_difference * transient_i);

  return sample();
}

CircuitSample PickupCircuit::sample() const {
  const double source = amplitude_v_ * std::cos(phase_);

  return {
      source,
      voltage_,
      source - voltage_,
      current_
  };
}

}  // namespace pickup::bsp::pc
