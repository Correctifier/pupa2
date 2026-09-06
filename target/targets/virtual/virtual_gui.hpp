#pragma once

#include <cstdint>
#include <string>

#include "simulated_pickup.hpp"

struct GLFWwindow;

namespace pickup::bsp::pc {

class VirtualGui {
 public:
  VirtualGui(
      std::uint16_t port,
      std::string serial_path,
      bool prefer_wayland
  );
  ~VirtualGui();

  VirtualGui(const VirtualGui&) = delete;

  VirtualGui& operator=(const VirtualGui&) = delete;

  bool should_close() const;
  // Edit a snapshot; the caller owns and applies the returned parameters.
  PickupParameters render(PickupParameters parameters);

 private:
  GLFWwindow* window_{};
  std::uint16_t port_;
  std::string serial_path_;
};

}  // namespace pickup::bsp::pc
