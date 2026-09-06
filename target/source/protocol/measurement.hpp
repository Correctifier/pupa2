#pragma once
#include <optional>

#include "module.hpp"
#include "signal_processing.hpp"

namespace pickup {
class Analyzer;
}

namespace pickup::protocol {
class MeasurementModule final : public Module {
 public:
  MeasurementModule(
      bsp::Transport& transport,
      Analyzer& analyzer,
      std::optional<std::uint32_t>& destination
  );
  ~MeasurementModule() override;

  MeasurementModule(const MeasurementModule&) = delete;

  MeasurementModule& operator=(const MeasurementModule&) = delete;

  void acquired(std::uint32_t endpoint, const ProcessedMeasurement& sample) const;
  void invalid_signal(std::uint32_t endpoint) const;

 private:
  Analyzer& analyzer_;
  std::optional<std::uint32_t>& destination_;
};
}  // namespace pickup::protocol
