cmake_minimum_required(VERSION 3.22)
project(pickup_analyzer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compilation database for clangd")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PICKUP_BUILD_VIRTUAL_TARGET "Build the desktop virtual target" ON)
option(PICKUP_BUILD_TESTS "Build unit tests" ON)
option(PICKUP_BUILD_STM32_TARGET "Build the STM32G4 target using the selected toolchain" OFF)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/ArduinoJson/CMakeLists.txt")
  message(FATAL_ERROR "Missing submodules. Run: git submodule update --init --recursive")
endif()

add_subdirectory(external/ArduinoJson)
add_library(pickup_application
  target/source/application.cpp
  target/source/protocol.cpp
  target/source/signal_processing.cpp
)
target_include_directories(pickup_application PUBLIC target/source)
target_link_libraries(pickup_application PUBLIC ArduinoJson)
target_compile_options(pickup_application PRIVATE -Wall -Wextra -Wpedantic)

if(PICKUP_BUILD_STM32_TARGET)
  target_compile_options(pickup_application PRIVATE
    -fno-exceptions
    -fno-rtti
    -ffunction-sections
    -fdata-sections
  )
  add_subdirectory(target/targets/stm32g4)
endif()

if(PICKUP_BUILD_VIRTUAL_TARGET)
  add_subdirectory(target/targets/virtual)
endif()

if(PICKUP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
