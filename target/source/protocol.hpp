#pragma once

#include "interfaces/impedance_frontend.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pickup::protocol {

struct Request {
  std::uint64_t transaction_id{};
  std::string type;
  double frequency_hz{};
};

std::optional<Request> parse_request(std::string_view line, std::string& error);
std::string make_measurement_response(std::uint64_t transaction_id,
                                      const bsp::ImpedanceSample& sample);
std::string make_info_response(std::uint64_t transaction_id);
std::string make_error_response(std::uint64_t transaction_id,
                                std::string_view error);

}  // namespace pickup::protocol

