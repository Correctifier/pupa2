#pragma once
#include <array>
#include <memory>
#include <optional>

#include "interfaces/impedance_analyzer.hpp"
#include "signal_processing.hpp"

namespace pickup {

// Callbacks run synchronously during tick(). Contexts must remain valid until their callbacks are
// removed. Callbacks must not reenter or destroy the analyzer or their context. Measurement
// references are borrowed only for the duration of the callback.
struct MeasurementCallbacks {
  void* context{};
  void (*measurement)(void*, const ProcessedMeasurement&){};
  void (*invalid_signal)(void*){};
};

struct SweepCallbacks {
  void* context{};
  void (*complete)(void*, std::uint32_t){};
};

struct SweepParameters {
  float start_hz{};
  float stop_hz{};
  std::uint32_t points{};
};

class Analyzer {
 public:
  explicit Analyzer(bsp::ImpedanceAnalyzer& hardware) : hardware_(hardware) {}

  Analyzer(const Analyzer&) = delete;

  Analyzer& operator=(const Analyzer&) = delete;

  bool supports_control(float frequency_hz, float amplitude_v) const;
  bool supports_frequency(float frequency_hz) const;
  bool set_generator(float frequency_hz, float amplitude_v);
  // Returns false when a sweep is already active.
  bool start_sweep(SweepParameters parameters, SweepCallbacks callbacks);
  void stop_sweep();
  void set_range_auto();
  bool set_range_manual(std::uint32_t index);
  // One measurement subscriber, independent of the active sweep.
  void set_measurement_callbacks(MeasurementCallbacks callbacks);
  void clear_measurement_callbacks(void* context);
  void tick();

 private:
  struct ActiveSweep {
    float start_hz{};
    float stop_hz{};
    std::uint32_t points{};
    std::uint32_t index{};
    bool frequency_set{};
    bool acquisition_started{};
    std::size_t processed_count{};
    float current_frequency_hz{};
    std::unique_ptr<AcquisitionProcessor> processor;
    SweepCallbacks callbacks;
  };
  bsp::ImpedanceAnalyzer& hardware_;
  MeasurementCallbacks measurement_callbacks_;
  std::optional<ActiveSweep> sweep_;
  static constexpr std::size_t acquisition_buffer_count_ =
      2 * FourthOrderMovingAverage::settling_frames;
  alignas(4) std::array<std::uint16_t, acquisition_buffer_count_> acquisition_buffer_{};
  std::uint32_t settling_time_ms_{};
  std::uint32_t control_set_at_ms_{};
  float control_amplitude_v_{0.25F};
};

}  // namespace pickup
