#include <ArduinoJson.h>

#include <cassert>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "application.hpp"
#include "protocol/calibration.hpp"
#include "protocol/device.hpp"
#include "protocol/generator.hpp"
#include "protocol/measurement.hpp"
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
  int generator_calls{};
  int range_calls{};
  int calibration_calls{};
  float frequency{};
  float amplitude{};
  bool automatic{};
  std::uint32_t range{};

  void set_control(float frequency_hz, float amplitude_v) override {
    ++generator_calls;
    frequency = frequency_hz;
    amplitude = amplitude_v;
  }

  bool start_acquisition(std::uint16_t*, std::size_t) override {
    return false;
  }

  std::size_t clean_data_count() const override {
    return 0;
  }

  bool acquisition_finished() const override {
    return false;
  }

  float sample_rate_hz() const override {
    return 64000;
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

template <typename Message>
Message decode_request(
    pickup::protocol::Module& module,
    std::string_view action,
    std::string_view params
) {
  StaticJsonDocument<256> document;
  const pickup::protocol::RequestContext context{
      42,
      7,
      module.object(),
      action
  };

  assert(deserializeJson(document, params) == DeserializationError::Ok);

  const auto decoded = module.decode(context, document.as<JsonVariantConst>());

  assert(decoded.error.ok());
  assert(decoded.message);

  const auto* message = std::get_if<Message>(&*decoded.message);

  assert(message);

  return *message;
}

void test_decoded_messages(Transport& transport) {
  Frontend hardware;
  pickup::Analyzer analyzer(hardware);
  pickup::Calibration calibration_service(hardware);
  pickup::protocol::DeviceModule device(transport, {
      "test",
      "test",
      "0"
  });
  pickup::protocol::GeneratorModule generator(transport, analyzer);
  pickup::protocol::SweepModule sweep(transport, analyzer);
  pickup::protocol::RangeModule range(transport, analyzer);
  pickup::protocol::CalibrationModule calibration(transport, calibration_service);

  const pickup::protocol::RequestContext context{
      42,
      7,
      "generator",
      "set"
  };
  const auto set_generator = pickup::protocol::GeneratorSetRequest{1234, 0.5F};
  pickup::protocol::MeasurementModule measurement(transport);

  assert(!device.handle(context, set_generator));
  assert(!sweep.handle(context, set_generator));
  assert(!range.handle(context, set_generator));
  assert(!calibration.handle(context, set_generator));
  assert(!measurement.handle(context, set_generator));
  assert(hardware.generator_calls == 0);
  assert(generator.handle(context, set_generator));
  assert(hardware.generator_calls == 1);

  // A domain error is handled, not a reason to try another module.
  const auto bad_range = pickup::protocol::RangeSetRequest{pickup::protocol::RangeMode::manual, 99};
  const auto rejected = range.handle(context, bad_range);
  StaticJsonDocument<1024> error;

  assert(rejected);
  assert(deserializeJson(error, rejected->view()) == DeserializationError::Ok);
  assert(error["error"]["code"] == "invalid_params");

  decode_request<pickup::protocol::DeviceInfoRequest>(
      device,
      "info",
      "{}"
  );

  // Values remain usable after the decoder's JSON document has been destroyed.
  const auto generator_message = decode_request<pickup::protocol::GeneratorSetRequest>(
      generator,
      "set",
      R"({"frequency":1234,"amplitude":0.5})"
  );
  const auto sweep_message = decode_request<pickup::protocol::SweepStartRequest>(
      sweep,
      "start",
      R"({"f_start":20,"f_stop":20000,"points":100})"
  );
  const auto range_message = decode_request<pickup::protocol::RangeSetRequest>(
      range,
      "set",
      R"({"mode":"manual","range":2})"
  );

  assert(generator_message.frequency_hz == 1234.0F);
  assert(generator_message.amplitude_v == 0.5F);
  assert(sweep_message.endpoint == 42);
  assert(sweep_message.start_hz == 20.0F);
  assert(sweep_message.stop_hz == 20000.0F);
  assert(sweep_message.points == 100);
  assert(range_message.mode == pickup::protocol::RangeMode::manual);
  assert(range_message.index == 2);
  decode_request<pickup::protocol::SweepStopRequest>(
      sweep,
      "stop",
      "{}"
  );
  decode_request<pickup::protocol::CalibrationRunRequest>(
      calibration,
      "run",
      "{}"
  );
}

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

  const pickup::AcquisitionUpdate update{
      pickup::ProcessedMeasurement{},
      true,
      1
  };
  const auto before = transport.outgoing.size();

  protocol.publish(update);
  assert(transport.outgoing.size() == before + 2);
  assert(transport.outgoing[before].endpoint == 17);
  assert(transport.outgoing[before + 1].endpoint == 17);
  protocol.publish(update);
  assert(transport.outgoing.size() == before + 2);

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

  protocol.publish(update);
  assert(transport.outgoing.size() == after_stop);
}
}  // namespace

int main() {
  Transport transport;

  test_decoded_messages(transport);
  test_sweep_event_destination();

  Frontend frontend;
  pickup::Application application({
      transport,
      frontend,
      {
          "target",
          "application",
          "1.2.3"
      }
  });
  pickup::Analyzer analyzer(frontend);
  pickup::protocol::SweepModule sweep(transport, analyzer);
  pickup::protocol::MeasurementModule measurement(transport);
  StaticJsonDocument<1024> reply;

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
  assert(reply["data"]["capabilities"].size() == 6);

  request(
      R"({"type":"request","object":"generator","action":"set","id":2,"params":{"frequency":1234,"amplitude":0.5}})"
  );
  assert(frontend.generator_calls == 1);
  assert(frontend.frequency == 1234.0F);
  assert(frontend.amplitude == 0.5F);
  assert(reply["id"] == 2);
  assert(reply["status"] == "ok");

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
