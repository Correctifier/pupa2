# Board-specific startup, linker script, CMSIS, and HAL integration belongs here.
# This directory is intentionally excluded from the host build. Configure it with
# an ARM toolchain file after selecting the exact STM32G4 Nucleo-32 board variant.
add_executable(pickup_stm32_target main.cpp)
target_link_libraries(pickup_stm32_target PRIVATE pickup_application)
target_compile_options(pickup_stm32_target PRIVATE
  -fno-exceptions
  -fno-rtti
  -ffunction-sections
  -fdata-sections
)
target_link_options(pickup_stm32_target PRIVATE -Wl,--gc-sections)
