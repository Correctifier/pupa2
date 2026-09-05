#include "application.hpp"
#include "signal_processing.hpp"

#include <cassert>
#include <cmath>
#include <complex>
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

class FakeFrontend final : public pickup::bsp::ImpedanceAnalyzer {
 public:
  void set_control(float, float) override {}
  bool start_acquisition(std::uint16_t*, std::size_t) override { return false; }
  std::size_t clean_data_count() const override { return 0; }
  bool acquisition_finished() const override { return false; }
  float sample_rate_hz() const override { return 192000; }
  void set_range_auto() override {}
  bool set_range_manual(std::uint32_t) override { return true; }
  std::uint32_t range_index() const override { return 0; }
  float sense_resistor_ohm() const override { return 100; }
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

  constexpr double pi = 3.14159265358979323846;
  constexpr double sample_rate = 64000.0;
  constexpr double frequency = 2000.0;
  constexpr std::size_t frames = 2048;
  std::array<std::uint16_t, frames * 2> samples{};
  const std::complex<float> expected_v(0.2F, 0.05F);
  const std::complex<float> expected_sense(0.04F, -0.01F);
  for (std::size_t index = 0; index < frames; ++index) {
    const double phase = 2 * pi * frequency * index / sample_rate;
    const std::complex<float> carrier(static_cast<float>(std::cos(phase)),
                                      static_cast<float>(std::sin(phase)));
    const auto adc = [](double volts) {
      return static_cast<std::uint16_t>(std::lround(2048.0 + volts * 4095.0 / 3.3));
    };
    samples[index * 2] = adc(std::real(expected_v * carrier));
    samples[index * 2 + 1] = adc(std::real(expected_sense * carrier));
  }
  pickup::AcquisitionProcessor processor;
  processor.begin(frequency, sample_rate);
  processor.process(samples.data(), samples.size());
  const auto measurement = processor.finish(2, 10000.0);
  assert(std::abs(measurement.v - expected_v) < 0.002);
  assert(std::abs(measurement.vsense - expected_sense) < 0.002);
  return 0;
}
