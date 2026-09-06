#include "calibration.hpp"

#include "../calibration.hpp"

namespace pickup::protocol {
DecodedMessage CalibrationModule::decode(const RequestContext& request, JsonVariantConst) {
  if (request.action != "run") {
    return {std::nullopt, unsupported_operation()};
  }

  return {CalibrationRunRequest{}, {}};
}

std::optional<EncodedMessage> CalibrationModule::handle(
    const RequestContext& request,
    const CalibrationRunRequest&
) {
  service_.run();

  return response(request);
}

}  // namespace pickup::protocol
