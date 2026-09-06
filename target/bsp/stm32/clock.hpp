#pragma once

namespace pickup::bsp::stm32 {
// Initialize HAL, SysTick, and the system clock before peripheral setup.
void initialize_clock();
}  // namespace pickup::bsp::stm32
