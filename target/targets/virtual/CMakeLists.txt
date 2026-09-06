if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp" OR
   NOT EXISTS "${PROJECT_SOURCE_DIR}/external/glfw/CMakeLists.txt")
  message(FATAL_ERROR "Virtual target requires imgui and glfw submodules")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/glfw ${CMAKE_BINARY_DIR}/glfw)
find_package(OpenGL REQUIRED)

add_library(imgui STATIC
  ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
  ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
  ${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
  ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
  ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
  ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${PROJECT_SOURCE_DIR}/external/imgui
  ${PROJECT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(pickup_virtual_target
  main.cpp
  virtual_gui.cpp
  ../../bsp/pc/posix_transport.cpp
  ../../bsp/pc/simulated_pickup.cpp
  ../../bsp/pc/pickup_circuit.cpp
)
target_include_directories(pickup_virtual_target PRIVATE ../../bsp/pc)
target_link_libraries(pickup_virtual_target PRIVATE pickup_application imgui)
target_compile_definitions(pickup_virtual_target PRIVATE
  PICKUP_APPLICATION_VERSION="${PROJECT_VERSION}")
target_compile_options(pickup_virtual_target PRIVATE -Wall -Wextra -Wpedantic)
