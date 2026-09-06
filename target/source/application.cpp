#include "application.hpp"

namespace pickup {

Application::Application(ApplicationDependencies dependencies)
    : analyzer_(dependencies.analyzer),
      calibration_(dependencies.analyzer),
      protocol_(
          dependencies.transport,
          dependencies.device,
          analyzer_,
          calibration_
      ) {}

void Application::tick() {
  protocol_.poll();

  if (const auto update = analyzer_.tick()) {
    protocol_.publish(*update);
  }
}

}  // namespace pickup
