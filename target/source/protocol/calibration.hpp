#pragma once
#include "module.hpp"

namespace pickup {
class Calibration;
}

namespace pickup::protocol {

class CalibrationModule final : public Module {
 public:
  CalibrationModule(bsp::Transport& transport, Calibration& service)
      : Module("calibration", transport), service_(service) {}

  EncodedMessage process(const RequestContext& request, JsonVariantConst params) override;

 private:
  Calibration& service_;
};

}  // namespace pickup::protocol
