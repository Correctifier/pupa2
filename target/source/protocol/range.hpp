#pragma once
#include "module.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {

class RangeModule final : public Module {
 public:
  RangeModule(bsp::Transport& transport, Analyzer& service)
      : Module("range", transport), service_(service) {}

  EncodedMessage process(const RequestContext& request, JsonVariantConst params) override;

 private:
  Analyzer& service_;
};

}  // namespace pickup::protocol
