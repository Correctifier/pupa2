if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
  message(FATAL_ERROR "Use -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake in a separate build directory")
endif()
if(PICKUP_BUILD_VIRTUAL_TARGET OR PICKUP_BUILD_TESTS)
  message(FATAL_ERROR "STM32 builds require PICKUP_BUILD_VIRTUAL_TARGET=OFF and PICKUP_BUILD_TESTS=OFF")
endif()

enable_language(ASM)
set(HAL_ROOT ${PROJECT_SOURCE_DIR}/external/stm32g4xx-hal-driver)
set(DEVICE_ROOT ${PROJECT_SOURCE_DIR}/external/cmsis-device-g4)
set(CORE_ROOT ${PROJECT_SOURCE_DIR}/external/CMSIS_5/CMSIS/Core)
foreach(required_file
    ${HAL_ROOT}/Src/stm32g4xx_hal.c
    ${DEVICE_ROOT}/Include/stm32g431xx.h
    ${CORE_ROOT}/Include/core_cm4.h)
  if(NOT EXISTS ${required_file})
    message(FATAL_ERROR "Missing STM32 dependency: ${required_file}. Run git submodule update --init --recursive")
  endif()
endforeach()

add_library(stm32g4_hal STATIC
  ${HAL_ROOT}/Src/stm32g4xx_hal_adc.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_adc_ex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_dac.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_dac_ex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_dma.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_dma_ex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_tim.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_tim_ex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_uart.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_uart_ex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_cortex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_flash.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_flash_ex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_gpio.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_pwr.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_pwr_ex.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_rcc.c
  ${HAL_ROOT}/Src/stm32g4xx_hal_rcc_ex.c
  ${DEVICE_ROOT}/Source/Templates/system_stm32g4xx.c
)
target_include_directories(stm32g4_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(stm32g4_hal SYSTEM PUBLIC
  ${HAL_ROOT}/Inc
  ${DEVICE_ROOT}/Include
  ${CORE_ROOT}/Include
)
target_compile_definitions(stm32g4_hal PUBLIC USE_HAL_DRIVER STM32G431xx)

add_executable(pickup_stm32_target
  main.cpp
  runtime.cpp
  ../../bsp/stm32/clock.cpp
  ../../bsp/stm32/board.cpp
  ../../bsp/stm32/serial_transport.cpp
  ../../bsp/stm32/generator.cpp
  ../../bsp/stm32/acquisition.cpp
  ../../bsp/stm32/ranges.cpp
  ../../bsp/stm32/frontend.cpp
  ${DEVICE_ROOT}/Source/Templates/gcc/startup_stm32g431xx.s
)
target_include_directories(pickup_stm32_target PRIVATE ../../bsp/stm32)
target_compile_definitions(pickup_stm32_target PRIVATE PICKUP_APPLICATION_VERSION="${PROJECT_VERSION}")
target_link_libraries(pickup_stm32_target PRIVATE pickup_application stm32g4_hal)
target_compile_options(pickup_stm32_target PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(pickup_stm32_target PROPERTIES SUFFIX .elf)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/stm32g431kb.ld)
set_property(TARGET pickup_stm32_target APPEND PROPERTY LINK_DEPENDS ${LINKER_SCRIPT})
target_link_options(pickup_stm32_target PRIVATE
  -T${LINKER_SCRIPT}
  -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/pickup_stm32_target.map
  -Wl,--print-memory-usage
)
add_custom_command(TARGET pickup_stm32_target POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:pickup_stm32_target> ${CMAKE_CURRENT_BINARY_DIR}/pickup_stm32_target.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pickup_stm32_target> ${CMAKE_CURRENT_BINARY_DIR}/pickup_stm32_target.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pickup_stm32_target>
  VERBATIM
)
