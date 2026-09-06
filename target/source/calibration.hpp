#pragma once
#include "interfaces/impedance_analyzer.hpp"

namespace pickup {

// Placeholder for calibration state and algorithms; retain the BSP operation.
class Calibration {
 public:
  explicit Calibration(bsp::ImpedanceAnalyzer& hardware) : hardware_(hardware) {}

  void run();

 private:
  bsp::ImpedanceAnalyzer& hardware_;
};

}  // namespace pickup
