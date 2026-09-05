# STM32 BSP

Implement the contracts in `target/source/interfaces` here. Application logic
must not include STM32 HAL headers; `target/targets/stm32g4/main.cpp` constructs
the BSP objects and injects them into `pickup::Application`.

