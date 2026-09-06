#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "interfaces/impedance_analyzer.hpp"
#include "interfaces/transport.hpp"
#include "signal_processing.hpp"

namespace pickup {

struct DeviceInformation {
  std::string_view target_name;
  std::string_view application_name;
  std::string_view application_version;
};

struct ApplicationDependencies {
  bsp::Transport& transport;
  bsp::ImpedanceAnalyzer& analyzer;
  DeviceInformation device;
};

class Application {
 public:
  explicit Application(ApplicationDependencies dependencies);
  void tick();

 private:
  struct ActiveSweep {
    std::uint32_t endpoint{};
    float start_hz{};
    float stop_hz{};
    std::uint32_t points{};
    std::uint32_t index{};
    bool acquisition_started{};
    std::size_t processed_count{};
    float current_frequency_hz{};
    AcquisitionProcessor processor;
  };
  void process_sweep();
  ApplicationDependencies dependencies_;
  std::optional<ActiveSweep> sweep_;
  static constexpr std::size_t acquisition_buffer_count_ = 4096;
  std::array<std::uint16_t, acquisition_buffer_count_> acquisition_buffer_{};
  float control_amplitude_v_{0.25F};
};

}  // namespace pickup
