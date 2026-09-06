#pragma once
#include "module.hpp"
#include "sweep_messages.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {

class SweepModule final : public Module {
 public:
  SweepModule(bsp::Transport& transport, Analyzer& service)
      : Module("sweep", transport), service_(service) {}

  using Module::handle;

  DecodedMessage decode(const RequestContext& request, JsonVariantConst params) override;
  void complete(std::uint32_t endpoint, std::uint32_t points) const;
  std::optional<EncodedMessage> handle(
      const RequestContext& request,
      const SweepStartRequest& message
  ) override;
  std::optional<EncodedMessage> handle(
      const RequestContext& request,
      const SweepStopRequest& message
  ) override;

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
