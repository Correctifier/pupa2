add_executable(application_tests application_tests.cpp)
target_link_libraries(application_tests PRIVATE pickup_application)
add_test(NAME application_tests COMMAND application_tests)

add_executable(simulator_sweep_tests
  simulator_sweep_tests.cpp
  ../target/bsp/pc/simulated_pickup.cpp
)
target_include_directories(simulator_sweep_tests PRIVATE ../target/bsp/pc)
target_link_libraries(simulator_sweep_tests PRIVATE pickup_application)
target_compile_options(simulator_sweep_tests PRIVATE -UNDEBUG)
add_test(NAME simulator_sweep_tests COMMAND simulator_sweep_tests)
