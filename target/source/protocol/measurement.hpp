#pragma once
#include "module.hpp"
#include "signal_processing.hpp"

namespace pickup::protocol {
class MeasurementModule final : public Module {
 public:
  explicit MeasurementModule(bsp::Transport& transport) : Module("measurement", transport) {}

  void acquired(std::uint32_t endpoint, const ProcessedMeasurement& sample) const;
  void invalid_signal(std::uint32_t endpoint) const;
};
}  // namespace pickup::protocol
