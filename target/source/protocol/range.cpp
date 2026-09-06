#include "range.hpp"

#include "../analyzer.hpp"

namespace pickup::protocol {
EncodedMessage RangeModule::process(const RequestContext& request, JsonVariantConst params) {
  if (request.action != "set") {
    return response(request, unsupported_operation());
  }

  const std::string_view mode = params["mode"] | "";

  if (mode == "auto") {
    service_.set_range_auto();
  } else if (mode == "manual") {
    if (!params["range"].is<std::uint32_t>()) {
      return response(request, {"invalid_params", "manual mode requires range"});
    }

    if (!service_.set_range_manual(params["range"].as<std::uint32_t>())) {
      return response(request, {"invalid_params", "range index is not available"});
    }
  } else {
    return response(request, {"invalid_params", "mode must be auto or manual"});
  }

  return response(request);
}

}  // namespace pickup::protocol
