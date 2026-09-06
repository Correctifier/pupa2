#pragma once

#include <complex>
#include <random>

#include "interfaces/impedance_analyzer.hpp"

namespace pickup::bsp::pc {

struct PickupParameters {
  double dcr_ohm{7000.0};
  double inductance_h{3.0};
  double capacitance_pf{120.0};
  double noise_percent{0.15};
};

class SimulatedPickup final : public ImpedanceAnalyzer {
 public:
  PickupParameters& parameters() {
    return parameters_;
  }
  void set_control(float frequency_hz, float amplitude_v) override;
  bool start_acquisition(std::uint16_t* buffer, std::size_t buffer_count) override;
  std::size_t clean_data_count() const override;
  bool acquisition_finished() const override;
  float sample_rate_hz() const override {
    return sample_rate_hz_;
  }
  void set_range_auto() override {
    automatic_range_ = true;
  }
  bool set_range_manual(std::uint32_t range_index) override;
  std::uint32_t range_index() const override {
    return range_index_;
  }
  float sense_resistor_ohm() const override;
  void calibrate() override {}

 private:
  PickupParameters parameters_;
  void advance_dma() const;
  float generator_frequency_hz_{1000.0F};
  float generator_amplitude_v_{0.25F};
  float sample_rate_hz_{192000.0F};
  bool automatic_range_{true};
  std::uint32_t range_index_{2};
  mutable std::mt19937 generator_{std::random_device{}()};
  mutable std::uint16_t* dma_buffer_{};
  mutable std::size_t dma_buffer_count_{};
  mutable std::size_t clean_count_{};
  mutable bool acquisition_active_{};
  mutable std::complex<double> v_signal_{};
  mutable std::complex<double> vsense_signal_{};
};

}  // namespace pickup::bsp::pc
