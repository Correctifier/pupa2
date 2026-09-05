#include "application.hpp"
#include "posix_transport.hpp"
#include "simulated_pickup.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

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
  pickup::bsp::pc::SimulatedPickup frontend;
  pickup::Application app({transport, frontend});

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

#if defined(__linux__)
  // Native Wayland does not provide applications with reliable minimize/
  // restore control. Prefer X11/XWayland when it is available so desktop
  // taskbar restoration behaves consistently. --wayland opts back in.
  if (!prefer_wayland && std::getenv("DISPLAY") != nullptr) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  }
#endif
  if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
  GLFWwindow* window = glfwCreateWindow(720, 390, "Pickup virtual target", nullptr, nullptr);
  if (!window) throw std::runtime_error("window creation failed");
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  const double dcr_min = 100.0, dcr_max = 30000.0;
  const double inductance_min = 0.01, inductance_max = 20.0;
  const double capacitance_min = 1.0, capacitance_max = 1000.0;
  const double noise_min = 0.0, noise_max = 10.0;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    app.tick();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Simulated pickup", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    auto& p = frontend.parameters();
    ImGui::Text("Endpoints");
    ImGui::BulletText("TCP: 127.0.0.1:%u", port);
    ImGui::BulletText("Serial: %s", transport.serial_path().c_str());
    ImGui::Separator();
    ImGui::SliderScalar("DCR (ohm)", ImGuiDataType_Double, &p.dcr_ohm,
                        &dcr_min, &dcr_max, "%.0f");
    ImGui::SliderScalar("Inductance (H)", ImGuiDataType_Double, &p.inductance_h,
                        &inductance_min, &inductance_max, "%.3f");
    ImGui::SliderScalar("Parallel capacitance (pF)", ImGuiDataType_Double,
                        &p.capacitance_pf, &capacitance_min, &capacitance_max, "%.1f");
    ImGui::SliderScalar("Noise (%)", ImGuiDataType_Double, &p.noise_percent,
                        &noise_min, &noise_max, "%.2f");
    ImGui::End();
    ImGui::Render();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
} catch (const std::exception& error) {
  std::cerr << "virtual target: " << error.what() << '\n';
  return 1;
}
