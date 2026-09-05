#include "application.hpp"

#include "protocol.hpp"

namespace pickup {

Application::Application(ApplicationDependencies dependencies)
    : dependencies_(dependencies) {}

void Application::tick() {
  while (auto line = dependencies_.transport.receive()) {
    std::string error;
    const auto request = protocol::parse_request(line->text, error);
    if (!request) {
      dependencies_.transport.send(line->endpoint,
                                   protocol::make_error_response(0, error));
    } else if (request->type == "measure_impedance") {
      const auto sample = dependencies_.frontend.measure(request->frequency_hz);
      dependencies_.transport.send(
          line->endpoint,
          protocol::make_measurement_response(request->transaction_id, sample));
    } else if (request->type == "get_info") {
      dependencies_.transport.send(
          line->endpoint, protocol::make_info_response(request->transaction_id));
    } else {
      dependencies_.transport.send(
          line->endpoint,
          protocol::make_error_response(request->transaction_id,
                                        "unsupported request type"));
    }
  }
}

}  // namespace pickup

