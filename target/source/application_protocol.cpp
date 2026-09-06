#include "application_protocol.hpp"

#include "analyzer.hpp"
#include "calibration.hpp"

namespace pickup {

ApplicationProtocol::ApplicationProtocol(
    bsp::Transport& transport,
    DeviceInformation device,
    Analyzer& analyzer,
    Calibration& calibration
)
    : Router(transport),
      device_(transport, device),
      generator_(transport, analyzer),
      sweep_(transport, analyzer),
      range_(transport, analyzer),
      calibration_(transport, calibration),
      measurement_(transport),
      modules_{
          &device_,
          &generator_,
          &sweep_,
          &range_,
          &calibration_,
          &measurement_
      } {
  register_modules(modules_);
}

protocol::EncodedMessage ApplicationProtocol::dispatch_message(
    const protocol::RequestContext& request,
    const ApplicationMessage& message
) {
  return std::visit(
      [this, &request](const auto& concrete_message) {
        for (auto* module : modules_) {
          if (auto response = module->handle(request, concrete_message)) {
            return *response;
          }
        }

        return protocol::response(request, protocol::unsupported_operation());
      },
      message
  );
}

void ApplicationProtocol::publish(const AcquisitionUpdate& update) {
  const auto endpoint = sweep_.endpoint();

  if (!endpoint) {
    return;
  }

  if (!update.measurement) {
    invalid_signal(*endpoint);
    sweep_.finish();

    return;
  }

  measurement_acquired(*endpoint, *update.measurement);

  if (update.complete) {
    sweep_complete(*endpoint, update.points);
    sweep_.finish();
  }
}

void ApplicationProtocol::measurement_acquired(
    std::uint32_t endpoint,
    const ProcessedMeasurement& sample
) const {
  measurement_.acquired(endpoint, sample);
}

void ApplicationProtocol::invalid_signal(std::uint32_t endpoint) const {
  measurement_.invalid_signal(endpoint);
}

void ApplicationProtocol::sweep_complete(std::uint32_t endpoint, std::uint32_t points) const {
  sweep_.complete(endpoint, points);
}

}  // namespace pickup
