#pragma once

#include "interfaces/impedance_analyzer.hpp"
#include "interfaces/transport.hpp"

namespace pickup::bsp::stm32 {

void initialize_board();
void heartbeat();

class SerialTransport final : public Transport {
 public:
  std::optional<ReceivedLine> receive() override;
  void send(std::uint32_t endpoint, std::string_view line) override;

 private:
  ReceivedLine line_;
  bool discard_line_{};
};

class Frontend final : public ImpedanceAnalyzer {
 public:
  std::uint32_t milliseconds() const override;
  bool supports_control(float frequency_hz, float amplitude_v) const override;
  void set_control(float frequency_hz, float amplitude_v) override;
  bool start_acquisition(std::uint16_t* buffer, std::size_t count) override;
  std::size_t clean_data_count() const override;
  bool acquisition_finished() const override;
  float sample_rate_hz() const override;
  void set_range_auto() override;
  bool set_range_manual(std::uint32_t index) override;
  std::uint32_t range_index() const override;
  float sense_resistor_ohm() const override;
  void calibrate() override;
};

}  // namespace pickup::bsp::stm32
