#pragma once
#include "module.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {

class GeneratorModule final : public Module {
 public:
  GeneratorModule(bsp::Transport& transport, Analyzer& service)
      : Module("generator", transport), service_(service) {}

  EncodedMessage process(const RequestContext& request, JsonVariantConst params) override;

 private:
  Analyzer& service_;
};

}  // namespace pickup::protocol
