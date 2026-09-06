#pragma once
#include "interfaces/transport.hpp"

namespace pickup::bsp::stm32 {
void initialize_serial();

class SerialTransport final : public Transport {
 public:
  std::optional<ReceivedLine> receive() override;
  void send(std::uint32_t endpoint, std::string_view line) override;

 private:
  ReceivedLine line_;
  bool discard_line_{};
};

}  // namespace pickup::bsp::stm32
