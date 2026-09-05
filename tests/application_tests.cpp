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
    return {frequency, 2, 10000.0, 0.2, 0.0, 0.04, -0.01,
            1000, 3000, 1500, 2500, 123.0, 456.0};
  }
  void set_generator(double, double) override {}
  void set_range_auto() override {}
  bool set_range_manual(std::uint32_t) override { return true; }
  void calibrate() override {}
};
}  // namespace

int main() {
  FakeTransport transport;
  FakeFrontend frontend;
  pickup::Application app({transport, frontend, {"test-target", "test-app", "0.0.0"}});
  transport.input.push_back(
      {42, R"({"type":"request","object":"device","action":"info","id":7})"});
  app.tick();
  assert(transport.output_endpoint == 42);
  assert(transport.output.find("\"id\":7") != std::string::npos);
  assert(transport.output.find("\"status\":\"ok\"") != std::string::npos);
  assert(transport.output.find("\"target_name\":\"test-target\"") != std::string::npos);
  assert(transport.output.find("\"application_version\":\"0.0.0\"") != std::string::npos);
  assert(transport.output.find("\"protocol_version\":1") != std::string::npos);
  return 0;
}
