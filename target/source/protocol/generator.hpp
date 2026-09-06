#pragma once
#include "generator_messages.hpp"
#include "module.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {

class GeneratorModule final : public Module {
 public:
  GeneratorModule(bsp::Transport& transport, Analyzer& service)
      : Module("generator", transport), service_(service) {}

  using Module::handle;

  DecodedMessage decode(const RequestContext& request, JsonVariantConst params) override;
  std::optional<EncodedMessage> handle(
      const RequestContext& request,
      const GeneratorSetRequest& message
  ) override;

 private:
  Analyzer& service_;
};

}  // namespace pickup::protocol
