#pragma once

#include "interfaces/impedance_frontend.hpp"
#include "interfaces/transport.hpp"

namespace pickup {

struct ApplicationDependencies {
  bsp::Transport& transport;
  bsp::ImpedanceFrontend& frontend;
};

class Application {
 public:
  explicit Application(ApplicationDependencies dependencies);
  void tick();

 private:
  ApplicationDependencies dependencies_;
};

}  // namespace pickup

