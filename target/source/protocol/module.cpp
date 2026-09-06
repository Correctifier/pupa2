#include "module.hpp"

namespace pickup::protocol {

EncodedMessage encode(const JsonDocument& document) {
  EncodedMessage message;

  if (document.overflowed() || measureJson(document) >= message.bytes.size() - 1) {
    return message;
  }

  const auto size = serializeJson(
      document,
      message.bytes.data(),
      message.bytes.size() - 1
  );
  message.bytes[size] = '\n';
  message.size = size + 1;

  return message;
}

EncodedMessage response(const RequestContext& request, Result result) {
  StaticJsonDocument<384> document;
  document["type"] = result.ok() ? "response" : "error";
  document["object"] = request.object;
  document["action"] = request.action;
  document["id"] = request.id;
  document["status"] = result.ok() ? "ok" : "error";

  if (!result.ok()) {
    auto error = document.createNestedObject("error");
    error["code"] = result.code;
    error["message"] = result.message;
  }

  return encode(document);
}

Result unsupported_operation() {
  return {"unsupported_operation", "unsupported object/action combination"};
}

EncodedMessage Module::process(const RequestContext& request, JsonVariantConst) {
  return response(request, unsupported_operation());
}

void Module::send(std::uint32_t endpoint, const EncodedMessage& message) const {
  if (message.size != 0) {
    transport_.send(endpoint, message.view());
  }
}

}  // namespace pickup::protocol
