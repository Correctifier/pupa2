#include "application.hpp"
#include "protocol.hpp"
#include <cmath>

namespace pickup {
Application::Application(ApplicationDependencies dependencies) : dependencies_(dependencies) {}

void Application::tick() {
  while (auto line = dependencies_.transport.receive()) {
    std::string code, message;
    const auto request = protocol::parse_request(line->text, code, message);
    if (!request) { dependencies_.transport.send(line->endpoint, protocol::error_response(0,"","",code,message)); continue; }
    const auto fail = [&](std::string_view c, std::string_view m) { dependencies_.transport.send(line->endpoint, protocol::error_response(request->id,request->object,request->action,c,m)); };
    if (request->object == "device" && request->action == "info") {
      dependencies_.transport.send(line->endpoint,
          protocol::device_info_response(*request, dependencies_.device.target_name,
              dependencies_.device.application_name, dependencies_.device.application_version));
    } else if (request->object == "generator" && request->action == "set") {
      dependencies_.frontend.set_generator(request->frequency,request->amplitude); dependencies_.transport.send(line->endpoint,protocol::response(*request));
    } else if (request->object == "sweep" && request->action == "start") {
      if (sweep_) fail("busy","a sweep is already running");
      else { sweep_=ActiveSweep{line->endpoint,request->f_start,request->f_stop,request->points,0}; dependencies_.transport.send(line->endpoint,protocol::response(*request)); }
    } else if (request->object == "sweep" && request->action == "stop") {
      sweep_.reset(); dependencies_.transport.send(line->endpoint,protocol::response(*request));
    } else if (request->object == "range" && request->action == "set") {
      bool ok=true; if(request->mode=="auto") dependencies_.frontend.set_range_auto(); else ok=dependencies_.frontend.set_range_manual(request->range);
      if(ok) dependencies_.transport.send(line->endpoint,protocol::response(*request)); else fail("invalid_params","range index is not available");
    } else if (request->object == "calibration" && request->action == "run") {
      dependencies_.frontend.calibrate(); dependencies_.transport.send(line->endpoint,protocol::response(*request));
    } else fail("unsupported_operation","unsupported object/action combination");
  }
  process_sweep();
}

void Application::process_sweep() {
  constexpr std::uint32_t max_points_per_tick = 16;
  for (std::uint32_t emitted=0; sweep_ && emitted<max_points_per_tick; ++emitted) {
    auto& sweep=*sweep_;
    const double fraction=static_cast<double>(sweep.index)/(sweep.points-1);
    const double frequency=std::exp(std::log(sweep.start_hz)+fraction*(std::log(sweep.stop_hz)-std::log(sweep.start_hz)));
    dependencies_.transport.send(sweep.endpoint,protocol::measurement_event(dependencies_.frontend.measure(frequency)));
    ++sweep.index;
    if(sweep.index==sweep.points) { dependencies_.transport.send(sweep.endpoint,protocol::sweep_event("complete",sweep.points)); sweep_.reset(); }
  }
}
}
