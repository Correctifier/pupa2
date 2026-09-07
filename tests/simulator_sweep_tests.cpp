#include <ArduinoJson.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "application.hpp"
#include "simulated_pickup.hpp"

namespace {
double simulated_time{};

double test_clock() {
  return simulated_time;
}

void test_frontend_timing_and_ranges() {
  simulated_time = 0;
  pickup::bsp::pc::SimulatedPickup frontend(test_clock);
  pickup::bsp::pc::PickupParameters parameters;
  parameters.noise_percent = 0;

  frontend.set_parameters(parameters);

  const std::array<float, 4> expected{
      1000,
      10000,
      100000,
      1000000
  };

  for (std::uint32_t index = 0; index < expected.size(); ++index) {
    assert(frontend.set_range_manual(index));
    assert(frontend.range_index() == index);
    assert(frontend.sense_resistor_ohm() == expected[index]);
    assert(frontend.status().sense_resistor_ohm == expected[index]);
    assert(!frontend.status().automatic_range);
  }

  assert(!frontend.set_range_manual(4));
  assert(frontend.range_index() == 3);
  assert(frontend.set_range_manual(2));
  frontend.set_control(1000, 0.5);
  assert(frontend.status().frequency_hz == 1000);
  assert(frontend.status().amplitude_v == 0.5);

  // Settling advances the circuit even with no DMA active.
  simulated_time = 0.050;
  std::array<std::uint16_t, 256> samples{};

  assert(frontend.start_acquisition(samples.data(), samples.size()));

  for (int poll = 0; poll < 20; ++poll) {
    assert(frontend.clean_data_count() == 0);
    assert(!frontend.acquisition_finished());
  }

  simulated_time += 0.001;

  assert(!frontend.start_acquisition(samples.data(), samples.size()));
  assert(!frontend.acquisition_finished());
  assert(frontend.clean_data_count() == 0);

  simulated_time += 0.001001;

  assert(frontend.clean_data_count() == samples.size());
  assert(frontend.acquisition_finished());

  // Compare a settled capture against the independent frequency-domain circuit.
  const std::complex<double> jw(0, 2 * 3.14159265358979323846 * 1000);
  const auto impedance = 1.0 / (1.0 / (7000.0 + jw * 3.0) + jw * 120e-12);
  const auto voltage = 0.5 * impedance / (100000.0 + impedance);
  const auto sense = 0.5 - voltage;

  for (std::size_t index = 0; index < samples.size() / 2; ++index) {
    const auto carrier = std::polar(1.0, 2 * 3.14159265358979323846 * (index + 1) / 64);
    const auto expected_v = std::lround(2048 + std::real(voltage * carrier) * 4095 / 3.3);
    const auto expected_sense = std::lround(2048 + std::real(sense * carrier) * 4095 / 3.3);

    assert(std::abs(static_cast<long>(samples[index * 2]) - expected_v) <= 1);
    assert(std::abs(static_cast<long>(samples[index * 2 + 1]) - expected_sense) <= 1);
  }

  // Automatic selection is opt-in and is applied before the next settling interval.
  frontend.set_range_auto();
  assert(frontend.status().automatic_range);
  assert(frontend.start_acquisition(samples.data(), samples.size()));

  simulated_time += 0.003;

  assert(frontend.acquisition_finished());
  assert(frontend.range_index() == 2);
  frontend.set_control(1000, 0.5);
  assert(frontend.range_index() == 1);
  assert(frontend.sense_resistor_ohm() == 10000);

  // A settings change during a capture must not publish mixed-range data.
  assert(frontend.start_acquisition(samples.data(), samples.size()));

  simulated_time += 0.001;

  assert(frontend.set_range_manual(3));

  simulated_time += 0.002;

  assert(frontend.acquisition_finished());
  assert(frontend.clean_data_count() == samples.size());

  for (const auto sample : samples) {
    assert(sample == 2048);
  }
}

class Transport final : public pickup::bsp::Transport {
 public:
  std::deque<pickup::bsp::ReceivedLine> incoming;
  std::vector<std::string> outgoing;

  std::optional<pickup::bsp::ReceivedLine> receive() override {
    if (incoming.empty()) {
      return std::nullopt;
    }

    auto line = incoming.front();

    incoming.pop_front();

    return line;
  }

  void send(std::uint32_t endpoint, std::string_view line) override {
    assert(endpoint == 42);
    outgoing.emplace_back(line);
  }

  void enqueue(std::string_view text) {
    pickup::bsp::ReceivedLine line;
    line.endpoint = 42;
    line.size = text.size();

    std::memcpy(
        line.text.data(),
        text.data(),
        text.size()
    );
    incoming.push_back(line);
  }
};
}  // namespace

int main() {
  test_frontend_timing_and_ranges();

  simulated_time = 0;
  Transport transport;
  pickup::bsp::pc::SimulatedPickup frontend(test_clock);
  pickup::bsp::pc::PickupParameters parameters;
  parameters.noise_percent = 0;

  frontend.set_parameters(parameters);
  assert(frontend.range_index() == 2);
  assert(frontend.sense_resistor_ohm() == 100000.0F);
  assert(!frontend.status().automatic_range);

  pickup::Application app({
      transport,
      frontend,
      {
          "test",
          "test",
          "0"
      }
  });

  for (const auto* params :
      {
          R"({"f_start":1000,"f_stop":1000,"points":2})",
          R"({"f_start":1000,"f_stop":2000,"points":1})",
          R"({"f_start":0,"f_stop":0,"points":1})",
          R"({"f_start":1000,"f_stop":1000,"points":0})",
          R"({"f_start":1e100,"f_stop":1e100,"points":1})"
      }) {
    const std::string request =
        std::string(R"({"type":"request","object":"sweep","action":"start","id":1,"params":)") +
        params + "}";

    transport.enqueue(request);
    app.tick();

    StaticJsonDocument<1024> invalid;

    assert(deserializeJson(invalid, transport.outgoing.back()) == DeserializationError::Ok);
    assert(invalid["id"] == 1);
    assert(invalid["object"] == "sweep");
    assert(invalid["action"] == "start");
    assert(invalid["error"]["code"] == "invalid_params");
  }

  transport.outgoing.clear();

  transport.enqueue(R"({"type":"request","object":"device","action":"info","id":0})");
  app.tick();

  StaticJsonDocument<1024> document;

  assert(deserializeJson(document, transport.outgoing.back()) == DeserializationError::Ok);
  assert(document["data"]["capabilities"].size() == 7);
  assert(document["data"]["capabilities"][5] == "single_point_sweep");
  assert(document["data"]["capabilities"][6] == "profiler");
  transport.enqueue(
      R"({"type":"request","object":"sweep","action":"start","id":99,"params":{"f_start":1000,"f_stop":2000,"points":1}})"
  );
  app.tick();
  assert(deserializeJson(document, transport.outgoing.back()) == DeserializationError::Ok);
  assert(document["type"] == "error");
  assert(document["id"].as<int>() == 99);
  assert(document["object"] == "sweep");
  assert(document["error"]["code"] == "invalid_params");
  transport.outgoing.clear();

  const auto start = [&] {
    transport.enqueue(
        R"({"type":"request","object":"sweep","action":"start","id":1,"params":{"f_start":1234,"f_stop":1234,"points":1}})"
    );
    app.tick();
  };

  start();
  // A second request must be rejected while the first sweep is settling.
  start();
  assert(transport.outgoing.back().find("busy") != std::string::npos);
  // Stop during settling, then immediately request another point.
  transport.enqueue(R"({"type":"request","object":"sweep","action":"stop","id":2})");
  app.tick();
  transport.outgoing.clear();
  start();

  for (int tick = 0; tick < 100; ++tick) {
    app.tick();

    simulated_time += 0.001;
  }

  assert(transport.outgoing.size() == 3);  // acknowledgement, measurement, completion
  assert(deserializeJson(document, transport.outgoing[1]) == DeserializationError::Ok);
  assert(document["object"] == "measurement");
  assert(document["data"]["f"].as<float>() == 1234.0F);
  assert(std::isfinite(document["data"]["z"]["re"].as<float>()));
  assert(deserializeJson(document, transport.outgoing[2]) == DeserializationError::Ok);
  assert(document["action"] == "complete");
  assert(document["data"]["points"].as<int>() == 1);

  transport.outgoing.clear();
  transport.enqueue(
      R"({"type":"request","object":"sweep","action":"start","id":3,"params":{"f_start":20,"f_stop":20000,"points":100}})"
  );

  for (int tick = 0; tick < 4000; ++tick) {
    app.tick();

    simulated_time += 0.001;
  }

  assert(transport.outgoing.size() == 102);
  assert(deserializeJson(document, transport.outgoing[1]) == DeserializationError::Ok);
  assert(document["data"]["f"].as<float>() == 20.0F);
  assert(deserializeJson(document, transport.outgoing[100]) == DeserializationError::Ok);
  assert(document["data"]["f"].as<float>() == 20000.0F);

  assert(frontend.range_index() == 2);
  assert(frontend.sense_resistor_ohm() == 100000.0F);
  assert(!frontend.status().automatic_range);

  return 0;
}
