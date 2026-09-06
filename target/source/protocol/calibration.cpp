#include "calibration.hpp"

#include "../calibration.hpp"

namespace pickup::protocol {
EncodedMessage CalibrationModule::process(const RequestContext& request, JsonVariantConst) {
  if (request.action != "run") {
    return response(request, unsupported_operation());
  }

  service_.run();

  return response(request);
}

}  // namespace pickup::protocol
