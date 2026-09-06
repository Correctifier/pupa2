#pragma once
#include <variant>

#include "protocol/calibration_messages.hpp"
#include "protocol/device_messages.hpp"
#include "protocol/generator_messages.hpp"
#include "protocol/range_messages.hpp"
#include "protocol/sweep_messages.hpp"

namespace pickup {

using ApplicationMessage = std::variant<
    protocol::DeviceInfoRequest,
    protocol::GeneratorSetRequest,
    protocol::SweepStartRequest,
    protocol::SweepStopRequest,
    protocol::RangeSetRequest,
    protocol::CalibrationRunRequest>;

}  // namespace pickup
