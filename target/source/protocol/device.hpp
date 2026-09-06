#pragma once
#include "device_information.hpp"
#include "device_messages.hpp"
#include "module.hpp"

namespace pickup::protocol {

class DeviceModule final : public Module {
 public:
  DeviceModule(bsp::Transport& transport, DeviceInformation info)
      : Module("device", transport), info_(info) {}

  using Module::handle;

  DecodedMessage decode(const RequestContext& request, JsonVariantConst params) override;
  std::optional<EncodedMessage> handle(
      const RequestContext& request,
      const DeviceInfoRequest& message
  ) override;

 private:
  DeviceInformation info_;
};

EncodedMessage response(const RequestContext& request, const DeviceInformation& info);

}  // namespace pickup::protocol
