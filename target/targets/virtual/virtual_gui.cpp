#include "virtual_gui.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace pickup::bsp::pc {

VirtualGui::VirtualGui(
    std::uint16_t port,
    std::string serial_path,
    bool prefer_wayland
)
    : port_(port), serial_path_(std::move(serial_path)) {
#if defined(__linux__)
  // Native Wayland does not provide applications with reliable minimize/
  // restore control. Prefer X11/XWayland when it is available so desktop
  // taskbar restoration behaves consistently. --wayland opts back in.
  if (!prefer_wayland && std::getenv("DISPLAY") != nullptr) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  }
#endif
  if (!glfwInit()) {
    throw std::runtime_error("GLFW initialization failed");
  }

  window_ = glfwCreateWindow(
      720,
      460,
      "Pickup virtual target",
      nullptr,
      nullptr
  );

  if (!window_) {
    glfwTerminate();

    throw std::runtime_error("window creation failed");
  }

  glfwMakeContextCurrent(window_);
  // Pace redraws ourselves so vsync does not stall simulated DMA progress.
  glfwSwapInterval(0);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 130");
}

VirtualGui::~VirtualGui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window_);
  glfwTerminate();
}

bool VirtualGui::should_close() const {
  return glfwWindowShouldClose(window_);
}

PickupParameters VirtualGui::render(PickupParameters p, const SimulatorStatus& status) {
  const double dcr_min = 100.0, dcr_max = 30000.0;
  const double inductance_min = 0.01, inductance_max = 20.0;
  const double capacitance_min = 1.0, capacitance_max = 1000.0;
  const double parallel_loss_min = 10000.0, parallel_loss_max = 10000000.0;
  const double noise_min = 0.0, noise_max = 10.0;

  glfwPollEvents();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin(
      "Simulated pickup",
      nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
  );

  ImGui::Text("Endpoints");
  ImGui::BulletText("TCP: 127.0.0.1:%u", port_);
  ImGui::BulletText("Serial: %s", serial_path_.c_str());
  ImGui::Separator();
  ImGui::Text("Generator: %.3f Hz", static_cast<double>(status.frequency_hz));
  ImGui::Text("Amplitude: %.3f V peak", static_cast<double>(status.amplitude_v));
  ImGui::Text(
      "Range %u: %.0f ohm (%s)",
      status.range_index,
      static_cast<double>(status.sense_resistor_ohm),
      status.automatic_range ? "automatic" : "fixed"
  );
  ImGui::Separator();
  ImGui::SliderScalar(
      "DCR (ohm)",
      ImGuiDataType_Double,
      &p.dcr_ohm,
      &dcr_min,
      &dcr_max,
      "%.0f"
  );
  ImGui::SliderScalar(
      "Inductance (H)",
      ImGuiDataType_Double,
      &p.inductance_h,
      &inductance_min,
      &inductance_max,
      "%.3f"
  );
  ImGui::SliderScalar(
      "Parallel capacitance (pF)",
      ImGuiDataType_Double,
      &p.capacitance_pf,
      &capacitance_min,
      &capacitance_max,
      "%.1f"
  );
  ImGui::SliderScalar(
      "Parallel loss (ohm)",
      ImGuiDataType_Double,
      &p.parallel_loss_ohm,
      &parallel_loss_min,
      &parallel_loss_max,
      "%.0f",
      ImGuiSliderFlags_Logarithmic
  );
  ImGui::SliderScalar(
      "Noise (%)",
      ImGuiDataType_Double,
      &p.noise_percent,
      &noise_min,
      &noise_max,
      "%.2f"
  );
  ImGui::End();
  ImGui::Render();

  int width, height;

  glfwGetFramebufferSize(
      window_,
      &width,
      &height
  );
  glViewport(
      0,
      0,
      width,
      height
  );
  glClearColor(
      0.08f,
      0.09f,
      0.11f,
      1.0f
  );
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window_);

  return p;
}

}  // namespace pickup::bsp::pc
