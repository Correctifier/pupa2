#pragma once
#include <array>

#include "device_information.hpp"
#include "protocol.hpp"
#include "protocol/calibration.hpp"
#include "protocol/device.hpp"
#include "protocol/generator.hpp"
#include "protocol/measurement.hpp"
#include "protocol/range.hpp"
#include "protocol/sweep.hpp"

namespace pickup {

class Analyzer;
class Calibration;
struct AcquisitionUpdate;

class ApplicationProtocol final : public protocol::Router {
 public:
  ApplicationProtocol(
      bsp::Transport& transport,
      DeviceInformation device,
      Analyzer& analyzer,
      Calibration& calibration
  );

  ApplicationProtocol(const ApplicationProtocol&) = delete;

  ApplicationProtocol& operator=(const ApplicationProtocol&) = delete;

  void publish(const AcquisitionUpdate& update);

 private:
  void measurement_acquired(std::uint32_t endpoint, const ProcessedMeasurement& sample) const;
  void invalid_signal(std::uint32_t endpoint) const;
  void sweep_complete(std::uint32_t endpoint, std::uint32_t points) const;

  protocol::DeviceModule device_;
  protocol::GeneratorModule generator_;
  protocol::SweepModule sweep_;
  protocol::RangeModule range_;
  protocol::CalibrationModule calibration_;
  protocol::MeasurementModule measurement_;
  std::array<protocol::Module*, 6> modules_;
};

}  // namespace pickup
