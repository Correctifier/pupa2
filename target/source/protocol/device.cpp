#include "device.hpp"

namespace pickup::protocol {
EncodedMessage DeviceModule::process(const RequestContext& request, JsonVariantConst) {
  if (request.action != "info") {
    return response(request, unsupported_operation());
  }

  return response(request, info_);
}

EncodedMessage response(const RequestContext& request, const DeviceInformation& info) {
  StaticJsonDocument<768> document;
  document["type"] = "response";
  document["object"] = "device";
  document["action"] = "info";
  document["id"] = request.id;
  document["status"] = "ok";
  auto data = document.createNestedObject("data");
  data["target_name"] = info.target_name;
  data["application_name"] = info.application_name;
  data["application_version"] = info.application_version;
  data["protocol_version"] = 1;
  auto capabilities = data.createNestedArray("capabilities");

  capabilities.add("generator");
  capabilities.add("sweep");
  capabilities.add("range");
  capabilities.add("calibration");
  capabilities.add("measurement_events");
  capabilities.add("single_point_sweep");
  capabilities.add("profiler");

  return encode(document);
}

}  // namespace pickup::protocol
