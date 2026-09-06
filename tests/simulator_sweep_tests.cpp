#include <ArduinoJson.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "application.hpp"
#include "protocol.hpp"
#include "simulated_pickup.hpp"

namespace {
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
  std::string_view code, error;
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
    assert(!pickup::protocol::parse_request(
        request,
        code,
        error
    ));
    assert(code == "invalid_params");
  }

  Transport transport;
  pickup::bsp::pc::SimulatedPickup frontend;
  frontend.parameters().noise_percent = 0;
  pickup::Application app({
      transport,
      frontend,
      {
          "test",
          "test",
          "0"
      }
  });
  transport.enqueue(R"({"type":"request","object":"device","action":"info","id":0})");
  app.tick();
  StaticJsonDocument<1024> document;
  assert(deserializeJson(document, transport.outgoing.back()) == DeserializationError::Ok);
  assert(document["data"]["capabilities"].size() == 6);
  assert(document["data"]["capabilities"][5] == "single_point_sweep");
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
  // A second request must be rejected while the first acquisition is active.
  start();
  assert(transport.outgoing.back().find("busy") != std::string::npos);
  // Stop with an unfinished simulated DMA, then immediately request another point.
  transport.enqueue(R"({"type":"request","object":"sweep","action":"stop","id":2})");
  app.tick();
  transport.outgoing.clear();
  start();
  for (int tick = 0; tick < 100; ++tick) {
    app.tick();
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
  for (int tick = 0; tick < 1000; ++tick) {
    app.tick();
  }
  assert(transport.outgoing.size() == 102);
  assert(deserializeJson(document, transport.outgoing[1]) == DeserializationError::Ok);
  assert(document["data"]["f"].as<float>() == 20.0F);
  assert(deserializeJson(document, transport.outgoing[100]) == DeserializationError::Ok);
  assert(document["data"]["f"].as<float>() == 20000.0F);
  return 0;
}
