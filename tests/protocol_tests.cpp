#include <ArduinoJson.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "application.hpp"
#include "protocol/calibration.hpp"
#include "protocol/device.hpp"
#include "protocol/generator.hpp"
#include "protocol/measurement.hpp"
#include "protocol/profiler.hpp"
#include "protocol/range.hpp"
#include "protocol/sweep.hpp"

namespace {
class Transport final : public pickup::bsp::Transport {
 public:
  struct Sent {
    std::uint32_t endpoint;
    std::string text;
  };
  std::deque<pickup::bsp::ReceivedLine> incoming;
  std::vector<Sent> outgoing;

  std::optional<pickup::bsp::ReceivedLine> receive() override {
    if (incoming.empty()) {
      return std::nullopt;
    }

    const auto line = incoming.front();

    incoming.pop_front();

    return line;
  }

  void send(std::uint32_t endpoint, std::string_view text) override {
    outgoing.push_back({endpoint, std::string(text)});
  }

  void enqueue(std::uint32_t endpoint, std::string_view text) {
    pickup::bsp::ReceivedLine line;
    line.endpoint = endpoint;
    line.size = text.size();

    assert(text.size() <= line.text.size());
    std::memcpy(
        line.text.data(),
        text.data(),
        text.size()
    );
    incoming.push_back(line);
  }
};

class Frontend final : public pickup::bsp::ImpedanceAnalyzer {
 public:
  std::uint32_t now_ms{};
  float sample_rate{64000.0F};
  bool reject_control{};

  bool supports_control(float, float) const override {
    return !reject_control;
  }

  std::uint32_t milliseconds() const override {
    return now_ms;
  }

  int generator_calls{};
  int range_calls{};
  int calibration_calls{};
  float frequency{};
  float amplitude{};
  bool automatic{};
  bool acquire{};
  bool invalid_signal{};
  std::size_t sample_count{};
  std::uint32_t range{};

  void set_control(float frequency_hz, float amplitude_v) override {
    ++generator_calls;
    frequency = frequency_hz;
    amplitude = amplitude_v;
  }

  bool start_acquisition(std::uint16_t* buffer, std::size_t count) override {
    if (!acquire) {
      return false;
    }

    sample_count = count;

    for (std::size_t i = 0; i < count; i += 2) {
      const auto wave = std::cos(2.0 * 3.141592653589793 * frequency * (i / 2) / sample_rate_hz());
      buffer[i] = static_cast<std::uint16_t>(2048 + 200 * wave);
      buffer[i + 1] = invalid_signal ? 2048 : static_cast<std::uint16_t>(2048 + 100 * wave);
    }

    return true;
  }

  std::size_t clean_data_count() const override {
    return sample_count;
  }

  bool acquisition_finished() const override {
    return acquire;
  }

  float sample_rate_hz() const override {
    return sample_rate;
  }

  void set_range_auto() override {
    ++range_calls;
    automatic = true;
  }

  bool set_range_manual(std::uint32_t index) override {
    ++range_calls;
    automatic = false;
    range = index;

    return index <= 3;
  }

  std::uint32_t range_index() const override {
    return range;
  }

  float sense_resistor_ohm() const override {
    return 10000;
  }

  void calibrate() override {
    ++calibration_calls;
  }
};

class Profiler final : public pickup::bsp::Profiler {
 public:
  std::span<const pickup::bsp::ProfileContext> contexts() const override {
    return context_table;
  }

  std::span<pickup::bsp::ProfileStatistics> statistics(
      std::span<pickup::bsp::ProfileStatistics> output
  ) override {
    const auto count = std::min(output.size(), context_table.size());

    for (std::size_t index = 0; index < count; ++index) {
      output[index] = {
          static_cast<std::uint32_t>(index),
          10 + index,
          100.0 + index,
          10.0 + index,
          5.0 + index,
          20.0 + index,
          1.0 + index,
      };
    }

    return output.first(count);
  }

  void reset() override {
    ++reset_count;
  }

  std::size_t reset_count{};

 private:
  static constexpr std::array context_table{
      pickup::bsp::ProfileContext{
          0,
          255,
          pickup::bsp::ProfileContextType::task,
          "main"
      },
      pickup::bsp::ProfileContext{
          1,
          0,
          pickup::bsp::ProfileContextType::interrupt,
          "uart"
      },
      pickup::bsp::ProfileContext{
          2,
          1,
          pickup::bsp::ProfileContextType::interrupt,
          "dac dma"
      },
      pickup::bsp::ProfileContext{
          3,
          1,
          pickup::bsp::ProfileContextType::interrupt,
          "adc dma"
      },
      pickup::bsp::ProfileContext{
          4,
          1,
          pickup::bsp::ProfileContextType::interrupt,
          "adc"
      },
      pickup::bsp::ProfileContext{
          5,
          1,
          pickup::bsp::ProfileContextType::interrupt,
          "timer dac"
      },
      pickup::bsp::ProfileContext{
          6,
          15,
          pickup::bsp::ProfileContextType::interrupt,
          "systick"
      },
  };
};

void test_sweep_event_destination() {
  Transport transport;
  Frontend hardware;
  pickup::Analyzer analyzer(hardware);
  pickup::Calibration calibration(hardware);
  pickup::ApplicationProtocol protocol(
      transport,
      {
          "test",
          "test",
          "0"
      },
      analyzer,
      calibration
  );

  transport.enqueue(
      17,
      R"({"type":"request","object":"sweep","action":"start","id":1,"params":{"f_start":1000,"f_stop":1000,"points":1}})"
  );
  protocol.poll();
  transport.enqueue(
      23,
      R"({"type":"request","object":"sweep","action":"start","id":2,"params":{"f_start":2000,"f_stop":2000,"points":1}})"
  );
  protocol.poll();

  StaticJsonDocument<1024> reply;

  assert(transport.outgoing.back().endpoint == 23);
  assert(deserializeJson(reply, transport.outgoing.back().text) == DeserializationError::Ok);
  assert(reply["error"]["code"] == "busy");

  hardware.acquire = true;
  const auto before = transport.outgoing.size();

  analyzer.tick();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(transport.outgoing.size() == before + 2);
  assert(transport.outgoing[before].endpoint == 17);
  assert(transport.outgoing[before + 1].endpoint == 17);
  analyzer.tick();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(transport.outgoing.size() == before + 2);

  assert(deserializeJson(reply, transport.outgoing[before].text) == DeserializationError::Ok);
  assert(reply["object"] == "measurement");
  assert(deserializeJson(reply, transport.outgoing[before + 1].text) == DeserializationError::Ok);
  assert(reply["action"] == "complete");

  // A stop from another endpoint clears the original event destination.
  analyzer.stop_sweep();
  transport.enqueue(
      17,
      R"({"type":"request","object":"sweep","action":"start","id":3,"params":{"f_start":1000,"f_stop":1000,"points":1}})"
  );
  protocol.poll();
  transport.enqueue(23, R"({"type":"request","object":"sweep","action":"stop","id":4})");
  protocol.poll();

  const auto after_stop = transport.outgoing.size();

  analyzer.tick();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(transport.outgoing.size() == after_stop);

  hardware.invalid_signal = true;

  transport.enqueue(
      29,
      R"({"type":"request","object":"sweep","action":"start","id":5,"params":{"f_start":1000,"f_stop":1000,"points":1}})"
  );
  protocol.poll();

  const auto before_failure = transport.outgoing.size();

  analyzer.tick();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(transport.outgoing.size() == before_failure + 1);
  assert(transport.outgoing.back().endpoint == 29);
  assert(deserializeJson(reply, transport.outgoing.back().text) == DeserializationError::Ok);
  assert(reply["error"]["code"] == "invalid_signal");
  analyzer.tick();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(transport.outgoing.size() == before_failure + 1);
}

void test_settling_time() {
  Frontend hardware;
  pickup::Analyzer analyzer(hardware);
  hardware.acquire = true;
  hardware.now_ms = 0xFFFFFFFFU;

  assert(pickup::FourthOrderMovingAverage::settling_frames == 128);

  assert(analyzer.start_sweep({
      1000,
      2000,
      2
  }, {}));
  analyzer.tick();
  assert(hardware.sample_count == 0);
  assert(hardware.generator_calls == 1);

  hardware.now_ms += 1;

  analyzer.tick();
  assert(hardware.sample_count == 0);
  assert(hardware.generator_calls == 1);

  hardware.now_ms += 5;

  analyzer.tick();
  assert(hardware.sample_count == 2 * pickup::FourthOrderMovingAverage::settling_frames);

  hardware.sample_count = 0;
  hardware.sample_rate = 48000.0F;

  analyzer.tick();
  assert(hardware.frequency == 2000);
  assert(hardware.sample_count == 0);

  // 256 frames plus the DAC pipeline allowance take eight milliseconds.
  hardware.now_ms += 7;

  analyzer.tick();
  assert(hardware.sample_count == 2 * pickup::FourthOrderMovingAverage::settling_frames);

  // Finish the second point before starting another operation.
  analyzer.tick();

  assert(analyzer.start_sweep({
      1000,
      1000,
      1
  }, {}));

  hardware.sample_count = 0;

  analyzer.tick();
  analyzer.stop_sweep();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(hardware.sample_count == 0);
}

void test_callback_lifetime() {
  Transport transport;
  Frontend hardware;
  hardware.acquire = true;
  pickup::Analyzer analyzer(hardware);
  pickup::Calibration calibration(hardware);

  {
    pickup::ApplicationProtocol protocol(
        transport,
        {
            "test",
            "test",
            "0"
        },
        analyzer,
        calibration
    );

    transport.enqueue(
        17,
        R"({"type":"request","object":"sweep","action":"start","id":1,"params":{"f_start":1000,"f_stop":1000,"points":1}})"
    );
    protocol.poll();
  }

  const auto before = transport.outgoing.size();

  analyzer.tick();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(transport.outgoing.size() == before);
  assert(hardware.generator_calls == 0);
}

void test_independent_measurement_subscription() {
  Transport transport;
  Frontend hardware;
  hardware.acquire = true;
  pickup::Analyzer analyzer(hardware);
  std::optional<std::uint32_t> destination{31};

  {
    // No SweepModule is involved in measurement registration or delivery.
    pickup::protocol::MeasurementModule measurement(
        transport,
        analyzer,
        destination
    );

    assert(analyzer.start_sweep({
        1000,
        1000,
        1
    }, {}));
    analyzer.tick();

    hardware.now_ms += 10;

    analyzer.tick();
    assert(transport.outgoing.size() == 1);
    assert(transport.outgoing.back().endpoint == 31);

    hardware.invalid_signal = true;

    assert(analyzer.start_sweep({
        1000,
        1000,
        1
    }, {}));
    analyzer.tick();

    hardware.now_ms += 10;

    analyzer.tick();
    assert(transport.outgoing.size() == 2);
    assert(transport.outgoing.back().endpoint == 31);
    assert(!destination);
  }

  // Destruction unregisters the callbacks while the analyzer remains usable.
  destination = 32;

  assert(analyzer.start_sweep({
      1000,
      1000,
      1
  }, {}));
  analyzer.tick();

  hardware.now_ms += 10;

  analyzer.tick();
  assert(transport.outgoing.size() == 2);
}
}  // namespace

int main() {
  Transport transport;

  test_settling_time();
  test_sweep_event_destination();
  test_callback_lifetime();
  test_independent_measurement_subscription();

  Frontend frontend;
  Profiler profiler;
  pickup::Application application({
      transport,
      frontend,
      {
          "target",
          "application",
          "1.2.3"
      },
      &profiler,
  });
  pickup::Analyzer analyzer(frontend);
  std::optional<std::uint32_t> destination;
  pickup::protocol::MeasurementModule measurement(
      transport,
      analyzer,
      destination
  );
  pickup::protocol::SweepModule sweep(
      transport,
      analyzer,
      destination
  );
  StaticJsonDocument<4096> reply;

  const auto request = [&](std::string_view text) {
    const auto previous_count = transport.outgoing.size();

    transport.enqueue(17, text);
    application.tick();
    assert(transport.outgoing.size() == previous_count + 1);
    assert(transport.outgoing.back().endpoint == 17);
    assert(transport.outgoing.back().text.back() == '\n');
    assert(deserializeJson(reply, transport.outgoing.back().text) == DeserializationError::Ok);
  };

  request(R"({"type":"request","object":"device","action":"info","id":18446744073709551615})");
  assert(reply["id"].as<std::uint64_t>() == UINT64_MAX);
  assert(reply["status"] == "ok");
  assert(reply["data"]["application_version"] == "1.2.3");
  assert(reply["data"]["capabilities"][6] == "profiler");

  request(R"({"type":"request","object":"profiler","action":"threads","id":200})");
  assert(reply["data"].size() == 7);
  assert(reply["data"][0]["name"] == "main");
  assert(reply["data"][1]["type"] == "interrupt");
  assert(reply["data"][1]["priority"] == 0);

  request(R"({"type":"request","object":"profiler","action":"data","id":201})");
  assert(reply["data"].size() == 7);
  assert(reply["data"][0]["count"] == 10);
  assert(reply["data"][1]["avg_us"] == 11.0);
  assert(reply["data"][1]["cpu_percent"] == 2.0);

  request(R"({"type":"request","object":"profiler","action":"reset","id":202})");
  assert(profiler.reset_count == 1);

  request(
      R"({"type":"request","object":"generator","action":"set","id":2,"params":{"frequency":1234,"amplitude":0.5}})"
  );
  assert(frontend.generator_calls == 1);
  assert(frontend.frequency == 1234.0F);

  assert(frontend.amplitude == 0.5F);
  assert(reply["id"] == 2);
  assert(reply["status"] == "ok");

  const auto accepted_generator_calls = frontend.generator_calls;
  frontend.reject_control = true;

  request(
      R"({"type":"request","object":"generator","action":"set","id":200,"params":{"frequency":30000,"amplitude":0.5}})"
  );
  assert(reply["error"]["code"] == "invalid_params");
  assert(frontend.generator_calls == accepted_generator_calls);
  request(
      R"({"type":"request","object":"sweep","action":"start","id":201,"params":{"f_start":20,"f_stop":30000,"points":10}})"
  );
  assert(reply["error"]["code"] == "invalid_params");
  assert(frontend.generator_calls == accepted_generator_calls);

  frontend.reject_control = false;

  for (const auto params :
      {
          R"({})",
          R"({"frequency":0,"amplitude":1})",
          R"({"frequency":1e100,"amplitude":1})"
      }) {
    request(
        std::string(R"({"type":"request","object":"generator","action":"set","id":3,"params":)") +
        params + "}"
    );
    assert(reply["error"]["code"] == "invalid_params");
    assert(reply["object"] == "generator");
    assert(reply["action"] == "set");
    assert(reply["id"] == 3);
    assert(frontend.generator_calls == 1);
  }

  request(R"({"type":"request","object":"range","action":"set","id":4,"params":{"mode":"auto"}})");
  assert(frontend.automatic);
  request(
      R"({"type":"request","object":"range","action":"set","id":5,"params":{"mode":"manual","range":2}})"
  );
  assert(frontend.range == 2);
  assert(!frontend.automatic);
  request(
      R"({"type":"request","object":"range","action":"set","id":6,"params":{"mode":"manual"}})"
  );
  assert(reply["error"]["code"] == "invalid_params");
  assert(frontend.range_calls == 2);
  request(
      R"({"type":"request","object":"range","action":"set","id":7,"params":{"mode":"manual","range":99}})"
  );
  assert(reply["type"] == "error");
  assert(reply["error"]["message"] == "range index is not available");

  request(
      R"({"type":"request","object":"sweep","action":"start","id":8,"params":{"f_start":1000,"f_stop":1000,"points":1}})"
  );
  assert(reply["status"] == "ok");
  assert(frontend.frequency == 1000.0F);

  request(
      R"({"type":"request","object":"sweep","action":"start","id":9,"params":{"f_start":20,"f_stop":20000,"points":100}})"
  );
  assert(reply["error"]["code"] == "busy");
  assert(reply["id"] == 9);
  request(R"({"type":"request","object":"sweep","action":"stop","id":10})");
  assert(reply["status"] == "ok");

  const auto calls_after_stop = frontend.generator_calls;

  application.tick();
  assert(frontend.generator_calls == calls_after_stop);
  // A successful restart proves stop cleared the active sweep.
  request(
      R"({"type":"request","object":"sweep","action":"start","id":15,"params":{"f_start":2000,"f_stop":2000,"points":1}})"
  );
  assert(reply["status"] == "ok");
  assert(frontend.frequency == 2000.0F);
  request(R"({"type":"request","object":"sweep","action":"stop","id":16})");
  request(R"({"type":"request","object":"calibration","action":"run","id":11})");
  assert(frontend.calibration_calls == 1);

  const auto calls_before_invalid = frontend.generator_calls;

  request(R"({"type":"request","object":"missing","action":"custom","id":13})");
  assert(reply["error"]["code"] == "unsupported_operation");
  assert(reply["id"] == 13);
  request(R"({"type":"request","object":"generator","action":"run","id":14})");
  assert(reply["error"]["code"] == "unsupported_operation");
  assert(frontend.generator_calls == calls_before_invalid);
  request("{");
  assert(reply["error"]["code"] == "malformed_json");
  request(R"({"type":"request","object":"device","action":"info","id":-1})");
  assert(reply["error"]["code"] == "invalid_envelope");

  // Batched requests retain their own transaction IDs and transport endpoints.
  const auto batch_start = transport.outgoing.size();

  transport.enqueue(23, R"({"type":"request","object":"device","action":"info","id":101})");
  transport.enqueue(24, R"({"type":"request","object":"calibration","action":"run","id":102})");
  application.tick();
  assert(transport.outgoing.size() == batch_start + 2);
  assert(transport.outgoing[batch_start].endpoint == 23);
  assert(deserializeJson(reply, transport.outgoing[batch_start].text) == DeserializationError::Ok);
  assert(reply["id"] == 101);
  assert(transport.outgoing.back().endpoint == 24);
  assert(deserializeJson(reply, transport.outgoing.back().text) == DeserializationError::Ok);
  assert(reply["id"] == 102);
  assert(frontend.calibration_calls == 2);

  pickup::ProcessedMeasurement sample;
  sample.frequency_hz = 1234;
  sample.range_index = 2;
  sample.sense_resistor_ohm = 10000;
  sample.v = {0.25F, -0.125F};
  sample.vsense = {0.125F, 0.25F};
  sample.impedance = {7000, 3000};
  sample.v_min = 1800;
  sample.v_max = 2300;
  sample.vsense_min = 1700;
  sample.vsense_max = 2400;

  measurement.acquired(42, sample);
  assert(transport.outgoing.back().endpoint == 42);
  assert(deserializeJson(reply, transport.outgoing.back().text) == DeserializationError::Ok);
  assert(reply["type"] == "event");
  assert(reply["object"] == "measurement");
  assert(!reply.containsKey("id"));
  assert(reply["data"]["f"] == 1234);
  assert(reply["data"]["v"]["im"] == -0.125F);
  assert(reply["data"]["vsense"]["re"] == 0.125F);
  assert(reply["data"]["z"]["im"] == 3000);
  assert(reply["data"]["vsense_max"] == 2400);

  sweep.complete(42, 100);
  assert(transport.outgoing.back().endpoint == 42);
  assert(deserializeJson(reply, transport.outgoing.back().text) == DeserializationError::Ok);
  assert(reply["type"] == "event");
  assert(reply["object"] == "sweep");
  assert(reply["action"] == "complete");
  assert(reply["data"]["points"] == 100);

  measurement.invalid_signal(42);
  assert(transport.outgoing.back().endpoint == 42);
  assert(deserializeJson(reply, transport.outgoing.back().text) == DeserializationError::Ok);
  assert(reply["type"] == "error");
  assert(reply["object"] == "measurement");
  assert(reply["action"] == "acquire");
  assert(reply["id"] == 0);
  assert(reply["error"]["code"] == "invalid_signal");

  return 0;
}
