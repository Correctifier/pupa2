#pragma once
#include <array>
#include <optional>

#include "device_information.hpp"
#include "protocol.hpp"
#include "protocol/calibration.hpp"
#include "protocol/device.hpp"
#include "protocol/generator.hpp"
#include "protocol/measurement.hpp"
#include "protocol/profiler.hpp"
#include "protocol/range.hpp"
#include "protocol/sweep.hpp"

namespace pickup {

class Analyzer;
class Calibration;

class ApplicationProtocol final : public protocol::Router {
 public:
  ApplicationProtocol(
      bsp::Transport& transport,
      DeviceInformation device,
      Analyzer& analyzer,
      Calibration& calibration,
      bsp::Profiler& profiler = bsp::null_profiler()
  );

  ApplicationProtocol(const ApplicationProtocol&) = delete;

  ApplicationProtocol& operator=(const ApplicationProtocol&) = delete;

 private:
  std::optional<std::uint32_t> sweep_destination_;
  protocol::DeviceModule device_;
  protocol::GeneratorModule generator_;
  protocol::MeasurementModule measurement_;
  protocol::SweepModule sweep_;
  protocol::RangeModule range_;
  protocol::CalibrationModule calibration_;
  protocol::ProfilerModule profiler_;
  std::array<protocol::Module*, 7> modules_;
};

}  // namespace pickup
