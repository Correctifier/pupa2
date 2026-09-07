add_executable(application_tests application_tests.cpp)
target_link_libraries(application_tests PRIVATE pickup_application)
add_test(NAME application_tests COMMAND application_tests)

add_executable(simulator_sweep_tests
  simulator_sweep_tests.cpp
  ../target/bsp/pc/simulated_pickup.cpp
  ../target/bsp/pc/pickup_circuit.cpp
)
target_include_directories(simulator_sweep_tests PRIVATE ../target/bsp/pc)
target_link_libraries(simulator_sweep_tests PRIVATE pickup_application)
target_compile_options(simulator_sweep_tests PRIVATE -UNDEBUG)
add_test(NAME simulator_sweep_tests COMMAND simulator_sweep_tests)

add_executable(protocol_tests protocol_tests.cpp)
target_link_libraries(protocol_tests PRIVATE pickup_application)
target_compile_options(protocol_tests PRIVATE -UNDEBUG)
add_test(NAME protocol_tests COMMAND protocol_tests)

add_executable(pickup_circuit_tests
  pickup_circuit_tests.cpp
  ../target/bsp/pc/pickup_circuit.cpp
)
target_include_directories(pickup_circuit_tests PRIVATE ../target/bsp/pc ../target/source)
target_compile_options(pickup_circuit_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
add_test(NAME pickup_circuit_tests COMMAND pickup_circuit_tests)

add_executable(stm32_waveform_tests
  stm32_waveform_tests.cpp
  ../target/bsp/stm32/nco.cpp
  ../target/bsp/stm32/gaussian_detector.cpp
)
target_include_directories(stm32_waveform_tests PRIVATE ../target/bsp/stm32)
target_link_libraries(stm32_waveform_tests PRIVATE pickup_application)
target_compile_options(stm32_waveform_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
add_test(NAME stm32_waveform_tests COMMAND stm32_waveform_tests)

add_executable(profiler_timing_tests profiler_timing_tests.cpp)
target_link_libraries(profiler_timing_tests PRIVATE pickup_application)
target_compile_options(profiler_timing_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
add_test(NAME profiler_timing_tests COMMAND profiler_timing_tests)
