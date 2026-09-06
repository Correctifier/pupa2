#pragma once
#include <optional>

#include "module.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {

class SweepModule final : public Module {
 public:
  SweepModule(
      bsp::Transport& transport,
      Analyzer& service,
      std::optional<std::uint32_t>& destination
  )
      : Module("sweep", transport), service_(service), endpoint_(destination) {}

  ~SweepModule() override;

  SweepModule(const SweepModule&) = delete;

  SweepModule& operator=(const SweepModule&) = delete;

  EncodedMessage process(const RequestContext& request, JsonVariantConst params) override;
  void complete(std::uint32_t endpoint, std::uint32_t points) const;

 private:
  Analyzer& service_;
  std::optional<std::uint32_t>& endpoint_;
};

}  // namespace pickup::protocol
