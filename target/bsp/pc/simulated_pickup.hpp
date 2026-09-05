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

 private:
  PickupParameters parameters_;
  std::mt19937 generator_{std::random_device{}()};
};

}  // namespace pickup::bsp::pc

