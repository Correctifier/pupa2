#pragma once

#include "interfaces/profiler.hpp"
#include "module.hpp"

namespace pickup::protocol {

class ProfilerModule final : public Module {
 public:
  ProfilerModule(bsp::Transport& transport, bsp::Profiler& profiler)
      : Module("profiler", transport), profiler_(profiler) {}

  EncodedMessage process(const RequestContext& request, JsonVariantConst params) override;

 private:
  EncodedMessage threads(const RequestContext& request) const;
  EncodedMessage data(const RequestContext& request);
  bsp::Profiler& profiler_;
};

}  // namespace pickup::protocol
