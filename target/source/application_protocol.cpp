#include "application_protocol.hpp"

#include "analyzer.hpp"
#include "calibration.hpp"

namespace pickup {

ApplicationProtocol::ApplicationProtocol(
    bsp::Transport& transport,
    DeviceInformation device,
    Analyzer& analyzer,
    Calibration& calibration,
    bsp::Profiler& profiler
)
    : Router(transport),
      device_(transport, device),
      generator_(transport, analyzer),
      measurement_(
          transport,
          analyzer,
          sweep_destination_
      ),
      sweep_(
          transport,
          analyzer,
          sweep_destination_
      ),
      range_(transport, analyzer),
      calibration_(transport, calibration),
      profiler_(transport, profiler),
      modules_{
          &device_,
          &generator_,
          &sweep_,
          &range_,
          &calibration_,
          &measurement_,
          &profiler_
      } {
  register_modules(modules_);
}

}  // namespace pickup
