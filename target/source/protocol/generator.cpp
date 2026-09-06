#include "generator.hpp"

#include <cmath>

#include "../analyzer.hpp"

namespace pickup::protocol {
DecodedMessage GeneratorModule::decode(const RequestContext& request, JsonVariantConst params) {
  if (request.action != "set") {
    return {std::nullopt, unsupported_operation()};
  }

  if (!params["frequency"].is<float>() || !params["amplitude"].is<float>()) {
    return {std::nullopt, {"invalid_params", "generator requires numeric frequency and amplitude"}};
  }

  const GeneratorSetRequest settings{
      params["frequency"].as<float>(),
      params["amplitude"].as<float>()
  };

  if (!std::isfinite(settings.frequency_hz) || !std::isfinite(settings.amplitude_v) ||
      settings.frequency_hz <= 0 || settings.amplitude_v <= 0) {
    return {std::nullopt, {"invalid_params", "generator values must be positive and finite"}};
  }

  return {settings, {}};
}

std::optional<EncodedMessage> GeneratorModule::handle(
    const RequestContext& request,
    const GeneratorSetRequest& message
) {
  service_.set_generator(message.frequency_hz, message.amplitude_v);

  return response(request);
}

}  // namespace pickup::protocol
