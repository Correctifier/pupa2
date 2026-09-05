#pragma once

#include "interfaces/impedance_frontend.hpp"

#include <random>

namespace pickup::bsp::pc {

struct PickupParameters {
  double dcr_ohm{7000.0};
  double inductance_h{3.0};
  double capacitance_pf{120.0};
  double noise_percent{0.15};
};

class SimulatedPickup final : public ImpedanceFrontend {
 public:
  PickupParameters& parameters() { return parameters_; }
  ImpedanceSample measure(double frequency_hz) override;
  void set_generator(double frequency_hz, double amplitude_v) override;
  void set_range_auto() override { automatic_range_ = true; }
  bool set_range_manual(std::uint32_t range_index) override;
  void calibrate() override {}

 private:
  PickupParameters parameters_;
  double generator_frequency_hz_{1000.0};
  double generator_amplitude_v_{0.25};
  bool automatic_range_{true};
  std::uint32_t range_index_{2};
  std::mt19937 generator_{std::random_device{}()};
};

}  // namespace pickup::bsp::pc
