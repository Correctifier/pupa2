#pragma once
#include "signal_processing.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pickup::protocol {
struct Request {
  std::uint64_t id{};
  std::string object;
  std::string action;
  float frequency{};
  float amplitude{};
  float f_start{};
  float f_stop{};
  std::uint32_t points{};
  std::string mode;
  std::uint32_t range{};
};
std::optional<Request> parse_request(std::string_view, std::string& code, std::string& error);
std::string response(const Request&, std::string_view data_key = {}, std::string_view data_value = {});
std::string device_info_response(const Request&, std::string_view target_name,
                                 std::string_view application_name,
                                 std::string_view application_version);
std::string error_response(std::uint64_t id, std::string_view object, std::string_view action,
                           std::string_view code, std::string_view message);
std::string measurement_event(const ProcessedMeasurement&);
std::string sweep_event(std::string_view action, std::uint32_t points);
}
