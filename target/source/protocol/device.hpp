#pragma once
#include "device_information.hpp"
#include "module.hpp"

namespace pickup::protocol {

class DeviceModule final : public Module {
 public:
  DeviceModule(bsp::Transport& transport, DeviceInformation info)
      : Module("device", transport), info_(info) {}

  EncodedMessage process(const RequestContext& request, JsonVariantConst params) override;

 private:
  DeviceInformation info_;
};

EncodedMessage response(const RequestContext& request, const DeviceInformation& info);

}  // namespace pickup::protocol
