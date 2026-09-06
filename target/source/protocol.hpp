#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "signal_processing.hpp"

namespace pickup::protocol {

enum class Object { unknown, device, generator, sweep, range, calibration, measurement };
enum class Action { unknown, info, set, start, stop, run, acquire, complete };
enum class RangeMode { unspecified, automatic, manual };

struct Request {
  std::uint64_t id{};
  Object object{Object::unknown};
  Action action{Action::unknown};
  float frequency{};
  float amplitude{};
  float f_start{};
  float f_stop{};
  std::uint32_t points{};
  RangeMode mode{RangeMode::unspecified};
  std::uint32_t range{};
};

struct EncodedMessage {
  static constexpr std::size_t capacity = 1024;
  std::array<char, capacity> bytes{};
  std::size_t size{};

  std::string_view view() const {
    return {bytes.data(), size};
  }
};

std::string_view to_string(Object value);
std::string_view to_string(Action value);
std::optional<Request> parse_request(
    std::string_view,
    std::string_view& code,
    std::string_view& error,
    Request* envelope = nullptr
);
EncodedMessage response(const Request&);
EncodedMessage device_info_response(
    const Request&,
    std::string_view target_name,
    std::string_view application_name,
    std::string_view application_version
);
EncodedMessage error_response(
    std::uint64_t id,
    Object object,
    Action action,
    std::string_view code,
    std::string_view message
);
EncodedMessage measurement_event(const ProcessedMeasurement&);
EncodedMessage sweep_event(Action action, std::uint32_t points);
}  // namespace pickup::protocol
