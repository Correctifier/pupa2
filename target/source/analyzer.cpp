#include "analyzer.hpp"

#include <algorithm>
#include <cmath>

namespace pickup {

void Analyzer::set_generator(float frequency_hz, float amplitude_v) {
  control_amplitude_v_ = amplitude_v;

  hardware_.set_control(frequency_hz, amplitude_v);
}

bool Analyzer::start_sweep(SweepParameters parameters) {
  if (sweep_) {
    return false;
  }

  sweep_.emplace();

  sweep_->start_hz = parameters.start_hz;
  sweep_->stop_hz = parameters.stop_hz;
  sweep_->points = parameters.points;

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

std::optional<AcquisitionUpdate> Analyzer::tick() {
  if (!sweep_) {
    return std::nullopt;
  }

  auto& sweep = *sweep_;

  if (!sweep.acquisition_started) {
    const float fraction =
        sweep.points == 1 ? 0.0F
                          : static_cast<float>(sweep.index) / static_cast<float>(sweep.points - 1);
    const float log_frequency =
        std::log(sweep.start_hz) + fraction * (std::log(sweep.stop_hz) - std::log(sweep.start_hz));
    sweep.current_frequency_hz =
        sweep.index == 0
            ? sweep.start_hz
            : (sweep.index + 1 == sweep.points ? sweep.stop_hz : std::exp(log_frequency));

    hardware_.set_control(sweep.current_frequency_hz, control_amplitude_v_);

    if (!hardware_.start_acquisition(acquisition_buffer_.data(), acquisition_buffer_.size())) {
      // Let an acquisition abandoned by sweep/stop finish before reusing its buffer.
      hardware_.acquisition_finished();

      return std::nullopt;
    }

    sweep.processor.begin(sweep.current_frequency_hz, hardware_.sample_rate_hz());

    sweep.processed_count = 0;
    sweep.acquisition_started = true;
  }

  std::size_t clean_count = std::min(hardware_.clean_data_count(), acquisition_buffer_.size());
  clean_count -= clean_count % 2;

  if (clean_count > sweep.processed_count) {
    sweep.processor.process(
        acquisition_buffer_.data() + sweep.processed_count,
        clean_count - sweep.processed_count
    );

    sweep.processed_count = clean_count;
  }

  const bool all_data_processed = sweep.processed_count == acquisition_buffer_.size();

  if (!hardware_.acquisition_finished() || !all_data_processed) {
    return std::nullopt;
  }

  const auto result =
      sweep.processor.finish(hardware_.range_index(), hardware_.sense_resistor_ohm());

  if (!result) {
    const AcquisitionUpdate update{
        std::nullopt,
        false,
        0
    };

    sweep_.reset();

    return update;
  }

  AcquisitionUpdate update{
      *result,
      false,
      sweep.points
  };

  ++sweep.index;
  sweep.acquisition_started = false;

  if (sweep.index == sweep.points) {
    update.complete = true;

    sweep_.reset();
  }

  return update;
}

}  // namespace pickup
