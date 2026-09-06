#pragma once
#include <ArduinoJson.h>

#include <array>
#include <cstdint>
#include <string_view>

#include "interfaces/transport.hpp"
#include "result.hpp"

namespace pickup::protocol {

struct RequestContext {
  std::uint32_t endpoint{};
  std::uint64_t id{};
  std::string_view object{"unknown"};
  std::string_view action{"unknown"};
};

struct EncodedMessage {
  std::array<char, 1024> bytes{};
  std::size_t size{};

  std::string_view view() const {
    return {bytes.data(), size};
  }
};

EncodedMessage encode(const JsonDocument& document);
EncodedMessage response(const RequestContext& request, Result result = Result{});
Result unsupported_operation();

// Context and JSON views are valid only for the synchronous process() call.
class Module {
 public:
  Module(std::string_view object, bsp::Transport& transport)
      : object_(object), transport_(transport) {}

  virtual ~Module() = default;

  std::string_view object() const {
    return object_;
  }

  virtual EncodedMessage process(const RequestContext& request, JsonVariantConst params);

 protected:
  void send(std::uint32_t endpoint, const EncodedMessage& message) const;

 private:
  std::string_view object_;
  bsp::Transport& transport_;
};

}  // namespace pickup::protocol
