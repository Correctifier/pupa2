#pragma once
#include <string_view>

namespace pickup {
struct DeviceInformation {
  std::string_view target_name;
  std::string_view application_name;
  std::string_view application_version;
};
}  // namespace pickup
