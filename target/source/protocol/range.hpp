#pragma once
#include "module.hpp"
#include "range_messages.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {

class RangeModule final : public Module {
 public:
  RangeModule(bsp::Transport& transport, Analyzer& service)
      : Module("range", transport), service_(service) {}

  using Module::handle;

  DecodedMessage decode(const RequestContext& request, JsonVariantConst params) override;
  std::optional<EncodedMessage> handle(
      const RequestContext& request,
      const RangeSetRequest& message
  ) override;

 private:
  Analyzer& service_;
};

}  // namespace pickup::protocol
