#include "sweep.hpp"

#include <cmath>

#include "../analyzer.hpp"

namespace pickup::protocol {
DecodedMessage SweepModule::decode(const RequestContext& request, JsonVariantConst params) {
  if (request.action == "stop") {
    return {SweepStopRequest{}, {}};
  }

  if (request.action != "start") {
    return {std::nullopt, unsupported_operation()};
  }

  if (!params["f_start"].is<float>() || !params["f_stop"].is<float>() ||
      !params["points"].is<std::uint32_t>()) {
    return {std::nullopt, {"invalid_params", "sweep requires f_start, f_stop, and points"}};
  }

  const SweepStartRequest settings{
      request.endpoint,
      params["f_start"].as<float>(),
      params["f_stop"].as<float>(),
      params["points"].as<std::uint32_t>()
  };
  const bool single_point = settings.points == 1 && settings.start_hz == settings.stop_hz;

  if (!std::isfinite(settings.start_hz) || !std::isfinite(settings.stop_hz) ||
      settings.start_hz <= 0 || settings.points == 0 || settings.points > 100000 ||
      (!single_point && (settings.stop_hz <= settings.start_hz || settings.points < 2))) {
    return {
        std::nullopt,
        {"invalid_params",
            "require positive finite endpoints, 2..100000 ascending points or 1 at equal endpoints"}
    };
  }

  return {settings, {}};
}

void SweepModule::complete(std::uint32_t endpoint, std::uint32_t points) const {
  StaticJsonDocument<192> document;
  document["type"] = "event";
  document["object"] = "sweep";
  document["action"] = "complete";
  document.createNestedObject("data")["points"] = points;

  send(endpoint, encode(document));
}

std::optional<EncodedMessage> SweepModule::handle(
    const RequestContext& request,
    const SweepStartRequest& message
) {
  if (!service_.start_sweep({
      message.start_hz,
      message.stop_hz,
      message.points
  })) {
    return response(request, {"busy", "a sweep is already running"});
  }

  endpoint_ = request.endpoint;

  return response(request);
}

std::optional<EncodedMessage> SweepModule::handle(
    const RequestContext& request,
    const SweepStopRequest&
) {
  service_.stop_sweep();
  endpoint_.reset();

  return response(request);
}

}  // namespace pickup::protocol
