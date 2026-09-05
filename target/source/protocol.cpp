#include "protocol.hpp"

#include <ArduinoJson.h>

namespace pickup::protocol {
namespace {
std::string encode(JsonDocument& document) {
  std::string text;
  serializeJson(document, text);
  text.push_back('\n');
  return text;
}
}  // namespace

std::optional<Request> parse_request(std::string_view line, std::string& code,
                                     std::string& error) {
  JsonDocument document;
  if (const auto result = deserializeJson(document, line); result) {
    code = "malformed_json";
    error = result.c_str();
    return std::nullopt;
  }
  if (document["type"] != "request" || !document["object"].is<const char*>() ||
      !document["action"].is<const char*>() || !document["id"].is<std::uint64_t>()) {
    code = "invalid_envelope";
    error = "request requires type, object, action, and unsigned id";
    return std::nullopt;
  }
  Request request;
  request.id = document["id"].as<std::uint64_t>();
  request.object = document["object"].as<std::string>();
  request.action = document["action"].as<std::string>();
  auto params = document["params"];
  if (request.object == "generator" && request.action == "set") {
    if (!params["frequency"].is<double>() || !params["amplitude"].is<double>()) {
      code = "invalid_params";
      error = "generator requires numeric frequency and amplitude";
      return std::nullopt;
    }
    request.frequency = params["frequency"];
    request.amplitude = params["amplitude"];
    if (request.frequency <= 0 || request.amplitude <= 0) {
      code = "invalid_params";
      error = "generator values must be positive";
      return std::nullopt;
    }
  } else if (request.object == "sweep" && request.action == "start") {
    if (!params["f_start"].is<double>() || !params["f_stop"].is<double>() ||
        !params["points"].is<std::uint32_t>()) {
      code = "invalid_params";
      error = "sweep requires f_start, f_stop, and points";
      return std::nullopt;
    }
    request.f_start = params["f_start"];
    request.f_stop = params["f_stop"];
    request.points = params["points"];
    if (request.f_start <= 0 || request.f_stop <= request.f_start || request.points < 2 ||
        request.points > 100000) {
      code = "invalid_params";
      error = "require 0 < f_start < f_stop and 2..100000 points";
      return std::nullopt;
    }
  } else if (request.object == "range" && request.action == "set") {
    if (!params["mode"].is<const char*>()) {
      code = "invalid_params";
      error = "range requires mode";
      return std::nullopt;
    }
    request.mode = params["mode"].as<std::string>();
    if (request.mode == "manual") {
      if (!params["range"].is<std::uint32_t>()) {
        code = "invalid_params";
        error = "manual mode requires range";
        return std::nullopt;
      }
      request.range = params["range"];
    } else if (request.mode != "auto") {
      code = "invalid_params";
      error = "mode must be auto or manual";
      return std::nullopt;
    }
  }
  return request;
}

std::string response(const Request& request, std::string_view data_key,
                     std::string_view data_value) {
  JsonDocument document;
  document["type"] = "response";
  document["object"] = request.object;
  document["action"] = request.action;
  document["id"] = request.id;
  document["status"] = "ok";
  if (!data_key.empty()) {
    document["data"][data_key] = data_value;
  }
  return encode(document);
}
std::string device_info_response(const Request& request, std::string_view target_name,
                                 std::string_view application_name,
                                 std::string_view application_version) {
  JsonDocument document;
  document["type"] = "response";
  document["object"] = request.object;
  document["action"] = request.action;
  document["id"] = request.id;
  document["status"] = "ok";

  auto data = document["data"].to<JsonObject>();
  data["target_name"] = target_name;
  data["application_name"] = application_name;
  data["application_version"] = application_version;
  data["protocol_version"] = 1;

  auto capabilities = data["capabilities"].to<JsonArray>();
  capabilities.add("generator");
  capabilities.add("sweep");
  capabilities.add("range");
  capabilities.add("calibration");
  capabilities.add("measurement_events");
  return encode(document);
}

std::string error_response(std::uint64_t id, std::string_view object, std::string_view action,
                           std::string_view code, std::string_view message) {
  JsonDocument document;
  document["type"] = "error";
  document["object"] = object;
  document["action"] = action;
  document["id"] = id;
  document["status"] = "error";
  document["error"]["code"] = code;
  document["error"]["message"] = message;
  return encode(document);
}

std::string measurement_event(const ProcessedMeasurement& s) {
  JsonDocument document;
  document["type"] = "event";
  document["object"] = "measurement";
  auto data = document["data"].to<JsonObject>();
  data["f"] = s.frequency_hz;
  data["range"] = s.range_index;
  data["rsense"] = s.sense_resistor_ohm;
  data["v"]["re"] = s.v.real();
  data["v"]["im"] = s.v.imag();
  data["vsense"]["re"] = s.vsense.real();
  data["vsense"]["im"] = s.vsense.imag();
  data["v_min"] = s.v_min;
  data["v_max"] = s.v_max;
  data["vsense_min"] = s.vsense_min;
  data["vsense_max"] = s.vsense_max;
  data["z"]["re"] = s.impedance.real();
  data["z"]["im"] = s.impedance.imag();
  return encode(document);
}

std::string sweep_event(std::string_view action, std::uint32_t points) {
  JsonDocument document;
  document["type"] = "event";
  document["object"] = "sweep";
  document["action"] = action;
  document["data"]["points"] = points;
  return encode(document);
}

}  // namespace pickup::protocol
