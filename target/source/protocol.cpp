#include "protocol.hpp"

#include <ArduinoJson.h>

namespace pickup::protocol {
namespace {
std::string serialize(JsonDocument& document) {
  std::string result;
  serializeJson(document, result);
  result.push_back('\n');
  return result;
}

JsonDocument response_base(std::string_view type, std::uint64_t id) {
  JsonDocument document;
  document["type"] = type;
  document["direction"] = "response";
  document["transaction_id"] = id;
  return document;
}
}  // namespace

std::optional<Request> parse_request(std::string_view line, std::string& error) {
  JsonDocument document;
  const auto result = deserializeJson(document, line);
  if (result) {
    error = result.c_str();
    return std::nullopt;
  }
  if (document["direction"] != "request" || !document["type"].is<const char*>() ||
      !document["transaction_id"].is<std::uint64_t>()) {
    error = "required fields: type, direction=request, transaction_id";
    return std::nullopt;
  }

  Request request;
  request.type = document["type"].as<std::string>();
  request.transaction_id = document["transaction_id"].as<std::uint64_t>();
  if (request.type == "measure_impedance") {
    if (!document["payload"]["frequency_hz"].is<double>()) {
      error = "payload.frequency_hz must be a number";
      return std::nullopt;
    }
    request.frequency_hz = document["payload"]["frequency_hz"].as<double>();
    if (request.frequency_hz <= 0.0) {
      error = "frequency_hz must be positive";
      return std::nullopt;
    }
  }
  return request;
}

std::string make_measurement_response(std::uint64_t id,
                                      const bsp::ImpedanceSample& sample) {
  auto document = response_base("measure_impedance", id);
  auto payload = document["payload"].to<JsonObject>();
  payload["frequency_hz"] = sample.frequency_hz;
  payload["real_ohm"] = sample.real_ohm;
  payload["imaginary_ohm"] = sample.imaginary_ohm;
  return serialize(document);
}

std::string make_info_response(std::uint64_t id) {
  auto document = response_base("get_info", id);
  auto payload = document["payload"].to<JsonObject>();
  payload["device"] = "pickup-analyzer";
  payload["protocol_version"] = 1;
  payload["capabilities"].to<JsonArray>().add("measure_impedance");
  return serialize(document);
}

std::string make_error_response(std::uint64_t id, std::string_view error) {
  auto document = response_base("error", id);
  document["payload"]["message"] = error;
  return serialize(document);
}
}  // namespace pickup::protocol

