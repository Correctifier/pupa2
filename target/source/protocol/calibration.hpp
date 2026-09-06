#pragma once
#include "calibration_messages.hpp"
#include "module.hpp"

namespace pickup {
class Calibration;
}

namespace pickup::protocol {

class CalibrationModule final : public Module {
 public:
  CalibrationModule(bsp::Transport& transport, Calibration& service)
      : Module("calibration", transport), service_(service) {}

  using Module::handle;

  DecodedMessage decode(const RequestContext& request, JsonVariantConst params) override;
  std::optional<EncodedMessage> handle(
      const RequestContext& request,
      const CalibrationRunRequest& message
  ) override;

 private:
  Calibration& service_;
};

}  // namespace pickup::protocol
