#include "application.hpp"

#include <cassert>
#include <deque>
#include <string>

namespace {
class FakeTransport final : public pickup::bsp::Transport {
 public:
  std::optional<pickup::bsp::ReceivedLine> receive() override {
    if (input.empty()) return std::nullopt;
    auto value = input.front();
    input.pop_front();
    return value;
  }
  void send(std::uint32_t endpoint, std::string_view line) override {
    output_endpoint = endpoint;
    output = line;
  }
  std::deque<pickup::bsp::ReceivedLine> input;
  std::uint32_t output_endpoint{};
  std::string output;
};

class FakeFrontend final : public pickup::bsp::ImpedanceFrontend {
 public:
  pickup::bsp::ImpedanceSample measure(double frequency) override {
    return {frequency, 123.0, 456.0};
  }
};
}  // namespace

int main() {
  FakeTransport transport;
  FakeFrontend frontend;
  pickup::Application app({transport, frontend});
  transport.input.push_back(
      {42, R"({"type":"measure_impedance","direction":"request","transaction_id":7,"payload":{"frequency_hz":1000}})"});
  app.tick();
  assert(transport.output_endpoint == 42);
  assert(transport.output.find("\"transaction_id\":7") != std::string::npos);
  assert(transport.output.find("\"real_ohm\":123") != std::string::npos);
  return 0;
}

