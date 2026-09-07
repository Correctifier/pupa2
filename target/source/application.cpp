#include "application.hpp"

namespace pickup {
namespace {
bsp::Profiler& profiler_or_null(bsp::Profiler* profiler) {
  return profiler == nullptr ? bsp::null_profiler() : *profiler;
}
}  // namespace

Application::Application(ApplicationDependencies dependencies)
    : analyzer_(dependencies.analyzer),
      calibration_(dependencies.analyzer),
      protocol_(
          dependencies.transport,
          dependencies.device,
          analyzer_,
          calibration_,
          profiler_or_null(dependencies.profiler)
      ) {}

void Application::tick() {
  protocol_.poll();

  analyzer_.tick();
}

}  // namespace pickup
