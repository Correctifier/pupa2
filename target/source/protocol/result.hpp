#pragma once
#include <string_view>

namespace pickup::protocol {

struct Result {
  std::string_view code;
  std::string_view message;

  bool ok() const {
    return code.empty();
  }
};

}  // namespace pickup::protocol
