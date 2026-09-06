#include "generator.hpp"

#include <cmath>

#include "../analyzer.hpp"

namespace pickup::protocol {
EncodedMessage GeneratorModule::process(const RequestContext& request, JsonVariantConst params) {
  if (request.action != "set") {
    return response(request, unsupported_operation());
  }

  if (!params["frequency"].is<float>() || !params["amplitude"].is<float>()) {
    return response(
        request,
        {"invalid_params", "generator requires numeric frequency and amplitude"}
    );
  }

  const float frequency_hz = params["frequency"].as<float>();
  const float amplitude_v = params["amplitude"].as<float>();

  if (!std::isfinite(frequency_hz) || !std::isfinite(amplitude_v) || frequency_hz <= 0 ||
      amplitude_v <= 0) {
    return response(request, {"invalid_params", "generator values must be positive and finite"});
  }

  if (!service_.set_generator(frequency_hz, amplitude_v)) {
    return response(request, {"invalid_params", "generator values exceed hardware limits"});
  }

  return response(request);
}

}  // namespace pickup::protocol
