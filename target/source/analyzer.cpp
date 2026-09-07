#include "analyzer.hpp"

#include <algorithm>
#include <cmath>

namespace pickup {

bool Analyzer::supports_control(float frequency_hz, float amplitude_v) const {
  return hardware_.supports_control(frequency_hz, amplitude_v);
}

bool Analyzer::supports_frequency(float frequency_hz) const {
  return supports_control(frequency_hz, control_amplitude_v_);
}

bool Analyzer::set_generator(float frequency_hz, float amplitude_v) {
  if (!supports_control(frequency_hz, amplitude_v)) {
    return false;
  }

  control_amplitude_v_ = amplitude_v;

  hardware_.set_control(frequency_hz, amplitude_v);

  control_set_at_ms_ = hardware_.milliseconds();
  // The continuously running STM32 DAC can have up to 512 previously generated
  // samples queued. Two milliseconds covers that 1.28 ms pipeline at 400 ksample/s;
  // other targets receive the same conservative allowance.
  constexpr std::uint32_t control_pipeline_ms = 2;
  constexpr float settling_cycles = 4.0F;
  const auto filter_settling_ms =
      static_cast<std::uint32_t>(std::ceil(1000.0F * settling_cycles / frequency_hz));
  settling_time_ms_ = control_pipeline_ms + filter_settling_ms;

  return true;
}

bool Analyzer::start_sweep(SweepParameters parameters, SweepCallbacks callbacks) {
  if (sweep_ || !supports_frequency(parameters.start_hz) ||
      !supports_frequency(parameters.stop_hz)) {
    return false;
  }

  sweep_.emplace();

  sweep_->start_hz = parameters.start_hz;
  sweep_->stop_hz = parameters.stop_hz;
  sweep_->points = parameters.points;
  sweep_->callbacks = callbacks;

  if (!hardware_.streams_demodulation()) {
    sweep_->processor = std::make_unique<AcquisitionProcessor>();
  }

  return true;
}

void Analyzer::stop_sweep() {
  sweep_.reset();
}

void Analyzer::set_range_auto() {
  hardware_.set_range_auto();
}

bool Analyzer::set_range_manual(std::uint32_t index) {
  return hardware_.set_range_manual(index);
}

void Analyzer::set_measurement_callbacks(MeasurementCallbacks callbacks) {
  measurement_callbacks_ = callbacks;
}

void Analyzer::clear_measurement_callbacks(void* context) {
  if (measurement_callbacks_.context == context) {
    measurement_callbacks_ = {};
  }
}

void Analyzer::tick() {
  if (!sweep_) {
    return;
  }

  auto& sweep = *sweep_;

  if (!sweep.frequency_set) {
    const float fraction =
        sweep.points == 1 ? 0.0F
                          : static_cast<float>(sweep.index) / static_cast<float>(sweep.points - 1);
    const float log_frequency =
        std::log(sweep.start_hz) + fraction * (std::log(sweep.stop_hz) - std::log(sweep.start_hz));
    sweep.current_frequency_hz =
        sweep.index == 0
            ? sweep.start_hz
            : (sweep.index + 1 == sweep.points ? sweep.stop_hz : std::exp(log_frequency));

    set_generator(sweep.current_frequency_hz, control_amplitude_v_);

    sweep.frequency_set = true;
  }

  if (!sweep.acquisition_started) {
    if (static_cast<std::uint32_t>(hardware_.milliseconds() - control_set_at_ms_) <
        settling_time_ms_) {
      return;
    }

    if (!hardware_.start_acquisition(acquisition_buffer_.data(), acquisition_buffer_.size())) {
      // Let an acquisition abandoned by sweep/stop finish before reusing its buffer.
      hardware_.acquisition_finished();

      return;
    }

    if (!hardware_.streams_demodulation()) {
      sweep.processor->begin(sweep.current_frequency_hz, hardware_.sample_rate_hz());
    }

    sweep.processed_count = 0;
    sweep.acquisition_started = true;
  }

  const bool streaming = hardware_.streams_demodulation();
  std::size_t clean_count = streaming
                                ? 0
                                : std::min(
                                      hardware_.clean_data_count(),
                                      acquisition_buffer_.size()
                                  );
  clean_count -= clean_count % 2;

  if (clean_count > sweep.processed_count) {
    sweep.processor->process(
        acquisition_buffer_.data() + sweep.processed_count,
        clean_count - sweep.processed_count
    );

    sweep.processed_count = clean_count;
  }

  const bool all_data_processed = streaming || sweep.processed_count == acquisition_buffer_.size();

  if (!hardware_.acquisition_finished() || !all_data_processed) {
    return;
  }

  std::optional<ProcessedMeasurement> result;

  if (streaming) {
    bsp::DemodulatedSignals signals;

    if (hardware_.read_demodulated(signals) && std::abs(signals.vsense) >= 1e-15F) {
      result = ProcessedMeasurement{
          sweep.current_frequency_hz,
          hardware_.range_index(),
          hardware_.sense_resistor_ohm(),
          signals.v,
          signals.vsense,
          signals.v_min,
          signals.v_max,
          signals.vsense_min,
          signals.vsense_max,
      };
      result->impedance = result->sense_resistor_ohm * result->v / result->vsense;
    }
  } else {
    result = sweep.processor->finish(hardware_.range_index(), hardware_.sense_resistor_ohm());
  }

  const auto callbacks = sweep.callbacks;
  const auto measurement_callbacks = measurement_callbacks_;

  if (!result) {
    sweep_.reset();

    if (measurement_callbacks.invalid_signal) {
      measurement_callbacks.invalid_signal(measurement_callbacks.context);
    }

    return;
  }

  const auto points = sweep.points;
  const bool complete = ++sweep.index == points;
  sweep.acquisition_started = false;
  sweep.frequency_set = false;

  if (complete) {
    sweep_.reset();
  }

  if (measurement_callbacks.measurement) {
    measurement_callbacks.measurement(measurement_callbacks.context, *result);
  }

  if (complete && callbacks.complete) {
    callbacks.complete(callbacks.context, points);
  }
}

}  // namespace pickup
