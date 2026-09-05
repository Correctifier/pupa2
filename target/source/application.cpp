#include "application.hpp"
#include "protocol.hpp"
#include <algorithm>
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
      control_amplitude_v_=static_cast<float>(request->amplitude);
      dependencies_.analyzer.set_control(static_cast<float>(request->frequency),control_amplitude_v_); dependencies_.transport.send(line->endpoint,protocol::response(*request));
    } else if (request->object == "sweep" && request->action == "start") {
      if (sweep_) fail("busy","a sweep is already running");
      else {
        sweep_.emplace(); sweep_->endpoint=line->endpoint; sweep_->start_hz=request->f_start;
        sweep_->stop_hz=request->f_stop; sweep_->points=request->points;
        dependencies_.transport.send(line->endpoint,protocol::response(*request));
      }
    } else if (request->object == "sweep" && request->action == "stop") {
      sweep_.reset(); dependencies_.transport.send(line->endpoint,protocol::response(*request));
    } else if (request->object == "range" && request->action == "set") {
      bool ok=true; if(request->mode=="auto") dependencies_.analyzer.set_range_auto(); else ok=dependencies_.analyzer.set_range_manual(request->range);
      if(ok) dependencies_.transport.send(line->endpoint,protocol::response(*request)); else fail("invalid_params","range index is not available");
    } else if (request->object == "calibration" && request->action == "run") {
      dependencies_.analyzer.calibrate(); dependencies_.transport.send(line->endpoint,protocol::response(*request));
    } else fail("unsupported_operation","unsupported object/action combination");
  }
  process_sweep();
}

void Application::process_sweep() {
  if (!sweep_) return;
  auto& sweep=*sweep_;
  if (!sweep.acquisition_started) {
    const float fraction=static_cast<float>(sweep.index)/static_cast<float>(sweep.points-1);
    sweep.current_frequency_hz=std::exp(std::log(sweep.start_hz)+fraction*(std::log(sweep.stop_hz)-std::log(sweep.start_hz)));
    dependencies_.analyzer.set_control(sweep.current_frequency_hz,control_amplitude_v_);
    if (!dependencies_.analyzer.start_acquisition(acquisition_buffer_.data(),acquisition_buffer_.size())) return;
    sweep.processor.begin(sweep.current_frequency_hz,dependencies_.analyzer.sample_rate_hz());
    sweep.processed_count=0; sweep.acquisition_started=true;
  }
  std::size_t clean=std::min(dependencies_.analyzer.clean_data_count(),acquisition_buffer_.size());
  clean-=clean%2;
  if(clean>sweep.processed_count) {
    sweep.processor.process(acquisition_buffer_.data()+sweep.processed_count,clean-sweep.processed_count);
    sweep.processed_count=clean;
  }
  if (dependencies_.analyzer.acquisition_finished() && sweep.processed_count==acquisition_buffer_.size()) {
    const auto result=sweep.processor.finish(dependencies_.analyzer.range_index(),dependencies_.analyzer.sense_resistor_ohm());
    dependencies_.transport.send(sweep.endpoint,protocol::measurement_event(result));
    ++sweep.index;
    sweep.acquisition_started=false;
    if(sweep.index==sweep.points) { dependencies_.transport.send(sweep.endpoint,protocol::sweep_event("complete",sweep.points)); sweep_.reset(); }
  }
}
}
