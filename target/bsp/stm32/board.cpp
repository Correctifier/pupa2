#include "board.hpp"

#include <cstdint>

#include "acquisition.hpp"
#include "clock.hpp"
#include "generator.hpp"
#include "hal_support.hpp"
#include "profiler.hpp"
#include "ranges.hpp"
#include "serial_transport.hpp"

namespace pickup::bsp::stm32::detail {
[[noreturn]] void fail() {
  __disable_irq();
  HAL_GPIO_WritePin(
      GPIOB,
      GPIO_PIN_8,
      GPIO_PIN_SET
  );

  while (true) {
    __WFI();
  }
}

void check(HAL_StatusTypeDef result) {
  if (result != HAL_OK) {
    fail();
  }
}
}  // namespace pickup::bsp::stm32::detail

namespace pickup::bsp::stm32 {
void initialize_board() {
  initialize_clock();
  profile::initialize();

  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitTypeDef gpio{};
  gpio.Pin = GPIO_PIN_8;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(GPIOB, &gpio);
  ranges::initialize();
  initialize_serial();
  generator::initialize();
  acquisition::initialize();
}

void wait_for_interrupt() {
  __WFI();
}

void heartbeat() {
  static std::uint32_t last_toggle{};
  const auto now = HAL_GetTick();

  if (now - last_toggle >= 500) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);

    last_toggle = now;
  }
}
}  // namespace pickup::bsp::stm32
