set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(CMAKE_C_COMPILER arm-none-eabi-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER arm-none-eabi-g++ REQUIRED)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc REQUIRED)
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(CMAKE_SIZE arm-none-eabi-size REQUIRED)

set(ARM_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT "${ARM_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${ARM_FLAGS} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
# Keep debug firmware within the G431KB flash while retaining useful stepping.
set(CMAKE_C_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_ASM_FLAGS_INIT "${ARM_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${ARM_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections")

set(PICKUP_BUILD_STM32_TARGET ON CACHE BOOL "Build STM32 target")
set(PICKUP_BUILD_VIRTUAL_TARGET OFF CACHE BOOL "Build host virtual target")
set(PICKUP_BUILD_TESTS OFF CACHE BOOL "Build host unit tests")
