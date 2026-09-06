#pragma once

#include <random>

#include "interfaces/impedance_analyzer.hpp"
#include "pickup_circuit.hpp"
#include "range_selection.hpp"

namespace pickup::bsp::pc {

struct SimulatorStatus {
  float frequency_hz{};
  float amplitude_v{};
  std::uint32_t range_index{};
  float sense_resistor_ohm{};
  bool automatic_range{};
};

class SimulatedPickup final : public ImpedanceAnalyzer {
 public:
  // Seconds from a monotonic clock; injectable for deterministic tests.
  using Clock = double (*)();

  explicit SimulatedPickup(Clock clock = steady_seconds);
  void set_parameters(PickupParameters parameters);
  SimulatorStatus status() const;
  std::uint32_t milliseconds() const override;
  void set_control(float frequency_hz, float amplitude_v) override;
  bool start_acquisition(std::uint16_t* buffer, std::size_t buffer_count) override;
  std::size_t clean_data_count() const override;
  bool acquisition_finished() const override;

  float sample_rate_hz() const override {
    return sample_rate_hz_;
  }

  void set_range_auto() override;
  bool set_range_manual(std::uint32_t range_index) override;

  std::uint32_t range_index() const override {
    return range_index_;
  }

  float sense_resistor_ohm() const override;

  void calibrate() override {}

 private:
  static double steady_seconds();
  void advance_to_now() const;
  void update_next_range() const;
  Clock clock_;
  PickupParameters parameters_;
  mutable PickupCircuit circuit_;
  mutable double circuit_time_{};
  float generator_frequency_hz_{1000.0F};
  float generator_amplitude_v_{0.25F};
  float sample_rate_hz_{64000.0F};
  bool automatic_range_{};
  std::uint32_t range_index_{startup_range_index};
  mutable std::uint32_t next_range_index_{startup_range_index};
  mutable std::mt19937 generator_{std::random_device{}()};
  mutable std::uint16_t* dma_buffer_{};
  mutable std::size_t dma_buffer_count_{};
  mutable std::size_t clean_count_{};
  mutable bool acquisition_active_{};
  mutable bool acquisition_invalid_{};
  mutable double next_sample_time_{};
  mutable double acquisition_sample_interval_{};
  mutable double voltage_power_{};
  mutable double sense_power_{};
};

}  // namespace pickup::bsp::pc
