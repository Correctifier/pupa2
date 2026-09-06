#pragma once

namespace pickup::bsp::stm32 {
void initialize_board();
void heartbeat();
void wait_for_interrupt();
}  // namespace pickup::bsp::stm32
