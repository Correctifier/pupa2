#include "range.hpp"

#include "../analyzer.hpp"

namespace pickup::protocol {
DecodedMessage RangeModule::decode(const RequestContext& request, JsonVariantConst params) {
  if (request.action != "set") {
    return {std::nullopt, unsupported_operation()};
  }

  const std::string_view mode = params["mode"] | "";
  RangeSetRequest settings;

  if (mode == "auto") {
    settings.mode = RangeMode::automatic;
  } else if (mode == "manual") {
    settings.mode = RangeMode::manual;

    if (!params["range"].is<std::uint32_t>()) {
      return {std::nullopt, {"invalid_params", "manual mode requires range"}};
    }

    settings.index = params["range"].as<std::uint32_t>();
  } else {
    return {std::nullopt, {"invalid_params", "mode must be auto or manual"}};
  }

  return {settings, {}};
}

std::optional<EncodedMessage> RangeModule::handle(
    const RequestContext& request,
    const RangeSetRequest& message
) {
  if (message.mode == RangeMode::automatic) {
    service_.set_range_auto();
  } else if (!service_.set_range_manual(message.index)) {
    return response(request, {"invalid_params", "range index is not available"});
  }

  return response(request);
}

}  // namespace pickup::protocol
