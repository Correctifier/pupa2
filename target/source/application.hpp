#pragma once

#include "interfaces/impedance_frontend.hpp"
#include "interfaces/transport.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace pickup {

struct DeviceInformation {
  std::string_view target_name;
  std::string_view application_name;
  std::string_view application_version;
};

struct ApplicationDependencies {
  bsp::Transport& transport;
  bsp::ImpedanceFrontend& frontend;
  DeviceInformation device;
};

class Application {
 public:
  explicit Application(ApplicationDependencies dependencies);
  void tick();

 private:
  struct ActiveSweep {
    std::uint32_t endpoint{};
    double start_hz{};
    double stop_hz{};
    std::uint32_t points{};
    std::uint32_t index{};
  };
  void process_sweep();
  ApplicationDependencies dependencies_;
  std::optional<ActiveSweep> sweep_;
};

}  // namespace pickup
