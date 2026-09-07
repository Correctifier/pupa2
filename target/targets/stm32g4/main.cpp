#include "application.hpp"
#include "generator.hpp"
#include "nucleo_g431kb.hpp"

int main() {
  pickup::bsp::stm32::initialize_board();

  static pickup::bsp::stm32::SerialTransport transport;
  static pickup::bsp::stm32::Frontend frontend;
  static pickup::bsp::stm32::PerformanceProfiler profiler;
  static pickup::Application app({
      transport,
      frontend,
      {
          "NUCLEO-G431KB",
          "Guitar Pickup Impedance Analyzer",
          PICKUP_APPLICATION_VERSION
      },
      &profiler,
  });

  pickup::bsp::stm32::generator::start(1000.0F, 0.25F);
  frontend.set_control(1000.0F, 0.25F);

  while (true) {
    pickup::bsp::stm32::profile::start(pickup::bsp::stm32::ProfileId::main_loop);
    app.tick();
    pickup::bsp::stm32::heartbeat();
    pickup::bsp::stm32::profile::stop(pickup::bsp::stm32::ProfileId::main_loop);
    pickup::bsp::stm32::wait_for_interrupt();
  }
}
