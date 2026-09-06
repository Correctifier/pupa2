#include "protocol.hpp"

#include <ArduinoJson.h>

#include <cmath>

namespace pickup::protocol {
namespace {

template <typename Document>
EncodedMessage encode(Document& document) {
  EncodedMessage message;
  const std::size_t json_size =
      serializeJson(
          document,
          message.bytes.data(),
          message.bytes.size() - 1
      );
  if (json_size == 0 || json_size >= message.bytes.size() - 1) {
    return message;
  }
  message.bytes[json_size] = '\n';
  message.size = json_size + 1;
  return message;
}

Object parse_object(std::string_view value) {
  if (value == "device") {
    return Object::device;
  }
  if (value == "generator") {
    return Object::generator;
  }
  if (value == "sweep") {
    return Object::sweep;
  }
  if (value == "range") {
    return Object::range;
  }
  if (value == "calibration") {
    return Object::calibration;
  }
  return Object::unknown;
}

Action parse_action(std::string_view value) {
  if (value == "info") {
    return Action::info;
  }
  if (value == "set") {
    return Action::set;
  }
  if (value == "start") {
    return Action::start;
  }
  if (value == "stop") {
    return Action::stop;
  }
  if (value == "run") {
    return Action::run;
  }
  return Action::unknown;
}

}  // namespace

std::string_view to_string(Object value) {
  switch (value) {
    case Object::device:
      return "device";
    case Object::generator:
      return "generator";
    case Object::sweep:
      return "sweep";
    case Object::range:
      return "range";
    case Object::calibration:
      return "calibration";
    case Object::measurement:
      return "measurement";
    default:
      return "unknown";
  }
}

std::string_view to_string(Action value) {
  switch (value) {
    case Action::info:
      return "info";
    case Action::set:
      return "set";
    case Action::start:
      return "start";
    case Action::stop:
      return "stop";
    case Action::run:
      return "run";
    case Action::acquire:
      return "acquire";
    case Action::complete:
      return "complete";
    default:
      return "unknown";
  }
}

std::optional<Request> parse_request(
    std::string_view line,
    std::string_view& code,
    std::string_view& error,
    Request* envelope
) {
  StaticJsonDocument<1024> document;
  if (deserializeJson(document, line) != DeserializationError::Ok) {
    code = "malformed_json";
    error = "invalid JSON or message exceeds fixed capacity";
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
  request.object = parse_object(document["object"].as<const char*>());
  request.action = parse_action(document["action"].as<const char*>());
  // Preserve transaction identity even when parameter validation fails.
  if (envelope) {
    *envelope = request;
  }
  const auto params = document["params"];

  if (request.object == Object::generator && request.action == Action::set) {
    if (!params["frequency"].is<float>() || !params["amplitude"].is<float>()) {
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
  } else if (request.object == Object::sweep && request.action == Action::start) {
    if (!params["f_start"].is<float>() || !params["f_stop"].is<float>() ||
        !params["points"].is<std::uint32_t>()) {
      code = "invalid_params";
      error = "sweep requires f_start, f_stop, and points";
      return std::nullopt;
    }
    request.f_start = params["f_start"];
    request.f_stop = params["f_stop"];
    request.points = params["points"];
    const bool single_point = request.points == 1 && request.f_start == request.f_stop;
    if (!std::isfinite(request.f_start) || !std::isfinite(request.f_stop) || request.f_start <= 0 ||
        request.points == 0 || request.points > 100000 ||
        (!single_point && (request.f_stop <= request.f_start || request.points < 2))) {
      code = "invalid_params";
      error =
          "require positive finite endpoints, 2..100000 ascending points or 1 at equal endpoints";
      return std::nullopt;
    }
  } else if (request.object == Object::range && request.action == Action::set) {
    const std::string_view mode = params["mode"] | "";
    if (mode == "auto") {
      request.mode = RangeMode::automatic;
    } else if (mode == "manual") {
      request.mode = RangeMode::manual;
      if (!params["range"].is<std::uint32_t>()) {
        code = "invalid_params";
        error = "manual mode requires range";
        return std::nullopt;
      }
      request.range = params["range"];
    } else {
      code = "invalid_params";
      error = "mode must be auto or manual";
      return std::nullopt;
    }
  }
  return request;
}

EncodedMessage response(const Request& request) {
  StaticJsonDocument<256> document;
  document["type"] = "response";
  document["object"] = to_string(request.object);
  document["action"] = to_string(request.action);
  document["id"] = request.id;
  document["status"] = "ok";
  return encode(document);
}

EncodedMessage device_info_response(
    const Request& request,
    std::string_view target_name,
    std::string_view application_name,
    std::string_view application_version
) {
  StaticJsonDocument<768> document;
  document["type"] = "response";
  document["object"] = "device";
  document["action"] = "info";
  document["id"] = request.id;
  document["status"] = "ok";
  auto data = document.createNestedObject("data");
  data["target_name"] = target_name;
  data["application_name"] = application_name;
  data["application_version"] = application_version;
  data["protocol_version"] = 1;
  auto capabilities = data.createNestedArray("capabilities");
  capabilities.add("generator");
  capabilities.add("sweep");
  capabilities.add("range");
  capabilities.add("calibration");
  capabilities.add("measurement_events");
  capabilities.add("single_point_sweep");
  return encode(document);
}

EncodedMessage error_response(
    std::uint64_t id,
    Object object,
    Action action,
    std::string_view code,
    std::string_view message
) {
  StaticJsonDocument<384> document;
  document["type"] = "error";
  document["object"] = to_string(object);
  document["action"] = to_string(action);
  document["id"] = id;
  document["status"] = "error";
  auto error = document.createNestedObject("error");
  error["code"] = code;
  error["message"] = message;
  return encode(document);
}

EncodedMessage measurement_event(const ProcessedMeasurement& sample) {
  StaticJsonDocument<768> document;
  document["type"] = "event";
  document["object"] = "measurement";
  auto data = document.createNestedObject("data");
  data["f"] = sample.frequency_hz;
  data["range"] = sample.range_index;
  data["rsense"] = sample.sense_resistor_ohm;
  auto v = data.createNestedObject("v");
  v["re"] = sample.v.real();
  v["im"] = sample.v.imag();
  auto vsense = data.createNestedObject("vsense");
  vsense["re"] = sample.vsense.real();
  vsense["im"] = sample.vsense.imag();
  data["v_min"] = sample.v_min;
  data["v_max"] = sample.v_max;
  data["vsense_min"] = sample.vsense_min;
  data["vsense_max"] = sample.vsense_max;
  auto impedance = data.createNestedObject("z");
  impedance["re"] = sample.impedance.real();
  impedance["im"] = sample.impedance.imag();
  return encode(document);
}

EncodedMessage sweep_event(Action action, std::uint32_t points) {
  StaticJsonDocument<192> document;
  document["type"] = "event";
  document["object"] = "sweep";
  document["action"] = to_string(action);
  document.createNestedObject("data")["points"] = points;
  return encode(document);
}

}  // namespace pickup::protocol
