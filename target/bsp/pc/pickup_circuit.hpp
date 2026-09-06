#pragma once

#include <complex>

namespace pickup::bsp::pc {

struct PickupParameters {
  double dcr_ohm{7000.0};
  double inductance_h{3.0};
  double capacitance_pf{120.0};
  double noise_percent{0.15};
};

struct CircuitSample {
  double source_v{};
  double v{};
  double vsense{};
  double inductor_current_a{};
};

// Ideal voltage source -> Rsense -> ((R + L) in parallel with C) -> ground.
// Physical state and source phase persist across coefficient changes.
class PickupCircuit {
 public:
  PickupCircuit();
  void set_parameters(PickupParameters parameters);
  void set_generator(double frequency_hz, double amplitude_v);
  void set_sense_resistor(double resistance_ohm);
  CircuitSample advance(double seconds);
  CircuitSample sample() const;

 private:
  void update_coefficients();
  PickupParameters parameters_;
  double sense_resistor_ohm_{};
  double frequency_hz_{1000.0};
  double amplitude_v_{0.25};
  double phase_{};
  double voltage_{};
  double current_{};
  double a_{}, b_{}, c_{}, d_{};
  std::complex<double> forced_voltage_{};
  std::complex<double> forced_current_{};
};

}  // namespace pickup::bsp::pc
