#pragma once
#include <array>
#include <optional>

#include "interfaces/impedance_analyzer.hpp"
#include "signal_processing.hpp"

namespace pickup {

// One acquisition can produce both a measurement and sweep completion.
// An update without a measurement reports an invalid sense signal.
struct AcquisitionUpdate {
  std::optional<ProcessedMeasurement> measurement;
  bool complete{};
  std::uint32_t points{};
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

  void set_generator(float frequency_hz, float amplitude_v);
  // Returns false when a sweep is already active.
  bool start_sweep(SweepParameters parameters);
  void stop_sweep();
  void set_range_auto();
  bool set_range_manual(std::uint32_t index);
  std::optional<AcquisitionUpdate> tick();

 private:
  struct ActiveSweep {
    float start_hz{};
    float stop_hz{};
    std::uint32_t points{};
    std::uint32_t index{};
    bool acquisition_started{};
    std::size_t processed_count{};
    float current_frequency_hz{};
    AcquisitionProcessor processor;
  };
  bsp::ImpedanceAnalyzer& hardware_;
  std::optional<ActiveSweep> sweep_;
  static constexpr std::size_t acquisition_buffer_count_ = 4096;
  std::array<std::uint16_t, acquisition_buffer_count_> acquisition_buffer_{};
  float control_amplitude_v_{0.25F};
};

}  // namespace pickup
