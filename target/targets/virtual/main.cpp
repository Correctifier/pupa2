#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>

#include "application.hpp"
#include "posix_transport.hpp"
#include "simulated_pickup.hpp"
#include "virtual_gui.hpp"

int main(int argc, char** argv) try {
  std::uint16_t port = 8765;
  bool headless = false;
  bool prefer_wayland = false;

  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--headless") {
      headless = true;
    } else if (std::string_view(argv[i]) == "--wayland") {
      prefer_wayland = true;
    } else {
      port = static_cast<std::uint16_t>(std::stoi(argv[i]));
    }
  }

  pickup::bsp::pc::PosixTransport transport(port);
  pickup::bsp::pc::PickupParameters parameters;
  pickup::bsp::pc::SimulatedPickup frontend;

  frontend.set_parameters(parameters);

  pickup::Application app({
      transport,
      frontend,
      {
          "PC virtual target",
          "Guitar Pickup Impedance Analyzer",
          PICKUP_APPLICATION_VERSION
      },
  });

  std::cout << "TCP: 127.0.0.1:" << port << '\n';
  std::cout << "Virtual serial: "
            << (transport.serial_path().empty() ? "unavailable" : transport.serial_path())
            << std::endl;

  if (headless) {
    while (true) {
      app.tick();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  pickup::bsp::pc::VirtualGui gui(
      port,
      transport.serial_path(),
      prefer_wayland
  );
  constexpr auto frame_interval = std::chrono::microseconds(16667);
  auto next_frame = std::chrono::steady_clock::now();

  while (!gui.should_close()) {
    app.tick();

    const auto now = std::chrono::steady_clock::now();

    if (now < next_frame) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

      continue;
    }

    next_frame = now + frame_interval;

    parameters = gui.render(parameters);

    frontend.set_parameters(parameters);
  }

  return 0;
} catch (const std::exception& error) {
  std::cerr << "virtual target: " << error.what() << '\n';

  return 1;
}
