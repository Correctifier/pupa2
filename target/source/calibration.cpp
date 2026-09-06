#include "calibration.hpp"

namespace pickup {

void Calibration::run() {
  hardware_.calibrate();
}

}  // namespace pickup
