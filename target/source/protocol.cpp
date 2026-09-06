#include "protocol.hpp"

namespace pickup::protocol {

void Router::poll() {
  while (auto line = transport_.receive()) {
    const auto message = dispatch(*line);

    if (message.size != 0) {
      transport_.send(line->endpoint, message.view());
    }
  }
}

EncodedMessage Router::dispatch(const bsp::ReceivedLine& line) {
  StaticJsonDocument<1024> document;
  RequestContext request;
  request.endpoint = line.endpoint;

  if (deserializeJson(document, line.view()) != DeserializationError::Ok) {
    return response(request, {"malformed_json", "invalid JSON or message exceeds fixed capacity"});
  }

  if (document["type"] != "request" || !document["object"].is<const char*>() ||
      !document["action"].is<const char*>() || !document["id"].is<std::uint64_t>()) {
    return response(
        request,
        {"invalid_envelope", "request requires type, object, action, and unsigned id"}
    );
  }

  request.id = document["id"].as<std::uint64_t>();
  request.object = document["object"].as<const char*>();
  request.action = document["action"].as<const char*>();

  for (auto* module : modules_) {
    if (module->object() == request.object) {
      const auto decoded = module->decode(request, document["params"].as<JsonVariantConst>());

      if (!decoded.message) {
        return response(request, decoded.error);
      }

      return dispatch_message(request, *decoded.message);
    }
  }

  return response(request, unsupported_operation());
}

}  // namespace pickup::protocol
