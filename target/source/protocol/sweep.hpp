#pragma once
#include <optional>

#include "module.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {

class SweepModule final : public Module {
 public:
  SweepModule(bsp::Transport& transport, Analyzer& service)
      : Module("sweep", transport), service_(service) {}

  EncodedMessage process(const RequestContext& request, JsonVariantConst params) override;
  void complete(std::uint32_t endpoint, std::uint32_t points) const;

  std::optional<std::uint32_t> endpoint() const {
    return endpoint_;
  }

  void finish() {
    endpoint_.reset();
  }

 private:
  Analyzer& service_;
  std::optional<std::uint32_t> endpoint_;
};

}  // namespace pickup::protocol
