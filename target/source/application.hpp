#pragma once

#include "analyzer.hpp"
#include "application_protocol.hpp"
#include "calibration.hpp"
#include "device_information.hpp"
#include "interfaces/impedance_analyzer.hpp"
#include "interfaces/transport.hpp"

namespace pickup {

struct ApplicationDependencies {
  bsp::Transport& transport;
  bsp::ImpedanceAnalyzer& analyzer;
  DeviceInformation device;
};

class Application {
 public:
  explicit Application(ApplicationDependencies dependencies);

  Application(const Application&) = delete;

  Application& operator=(const Application&) = delete;

  void tick();

 private:
  Analyzer analyzer_;
  Calibration calibration_;
  ApplicationProtocol protocol_;
};

}  // namespace pickup
