#include "sweep.hpp"

#include <cmath>

#include "../analyzer.hpp"

namespace pickup::protocol {
SweepModule::~SweepModule() {
  // Cancel borrowed callbacks before this module dies.
  if (endpoint_) {
    service_.stop_sweep();
  }
}

EncodedMessage SweepModule::process(const RequestContext& request, JsonVariantConst params) {
  if (request.action == "stop") {
    service_.stop_sweep();
    endpoint_.reset();

    return response(request);
  }

  if (request.action != "start") {
    return response(request, unsupported_operation());
  }

  if (!params["f_start"].is<float>() || !params["f_stop"].is<float>() ||
      !params["points"].is<std::uint32_t>()) {
    return response(request, {"invalid_params", "sweep requires f_start, f_stop, and points"});
  }

  const SweepParameters settings{
      params["f_start"].as<float>(),
      params["f_stop"].as<float>(),
      params["points"].as<std::uint32_t>()
  };
  const bool single_point = settings.points == 1 && settings.start_hz == settings.stop_hz;

  if (!std::isfinite(settings.start_hz) || !std::isfinite(settings.stop_hz) ||
      settings.start_hz <= 0 || settings.points == 0 || settings.points > 100000 ||
      (!single_point && (settings.stop_hz <= settings.start_hz || settings.points < 2))) {
    return response(
        request,
        {"invalid_params",
            "require positive finite endpoints, 2..100000 ascending points or 1 at equal endpoints"}
    );
  }

  if (!service_.supports_frequency(settings.start_hz) ||
      !service_.supports_frequency(settings.stop_hz)) {
    return response(request, {"invalid_params", "sweep frequencies exceed hardware limits"});
  }

  const SweepCallbacks callbacks{this, [](void* context, std::uint32_t points) {
                                   auto& self = *static_cast<SweepModule*>(context);
                                   const auto endpoint = *self.endpoint_;

                                   self.endpoint_.reset();

                                   self.complete(endpoint, points);
                                 }};

  if (!service_.start_sweep(settings, callbacks)) {
    return response(request, {"busy", "a sweep is already running"});
  }

  endpoint_ = request.endpoint;

  return response(request);
}

void SweepModule::complete(std::uint32_t endpoint, std::uint32_t points) const {
  StaticJsonDocument<192> document;
  document["type"] = "event";
  document["object"] = "sweep";
  document["action"] = "complete";
  document.createNestedObject("data")["points"] = points;

  send(endpoint, encode(document));
}

}  // namespace pickup::protocol
