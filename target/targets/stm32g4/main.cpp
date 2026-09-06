#include "application.hpp"
#include "nucleo_g431kb.hpp"
#include "stm32g4xx_hal.h"

namespace {
[[noreturn]] void clock_failure() {
  __disable_irq();

  while (true) {
    __WFI();
  }
}

void configure_clock() {
  __HAL_RCC_PWR_CLK_ENABLE();

  // HSI16 / 4 * 85 / 2 = 170 MHz; no external oscillator is required.
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK) {
    clock_failure();
  }

  RCC_OscInitTypeDef oscillator{};
  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  oscillator.HSIState = RCC_HSI_ON;
  oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  oscillator.PLL.PLLM = RCC_PLLM_DIV4;
  oscillator.PLL.PLLN = 85;
  oscillator.PLL.PLLP = RCC_PLLP_DIV2;
  oscillator.PLL.PLLQ = RCC_PLLQ_DIV2;
  oscillator.PLL.PLLR = RCC_PLLR_DIV2;

  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
    clock_failure();
  }

  RCC_ClkInitTypeDef clocks{};
  clocks.ClockType =
      RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV1;
  clocks.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_8) != HAL_OK) {
    clock_failure();
  }
}
}  // namespace

extern "C" void SysTick_Handler() {
  HAL_IncTick();
}

int main() {
  if (HAL_Init() != HAL_OK) {
    clock_failure();
  }

  configure_clock();

  pickup::bsp::stm32::initialize_board();

  static pickup::bsp::stm32::SerialTransport transport;
  static pickup::bsp::stm32::Frontend frontend;
  static pickup::Application app({
      transport,
      frontend,
      {
          "NUCLEO-G431KB",
          "Guitar Pickup Impedance Analyzer",
          PICKUP_APPLICATION_VERSION
      },
  });

  frontend.set_control(1000.0F, 0.25F);

  while (true) {
    app.tick();
    pickup::bsp::stm32::heartbeat();
    __WFI();
  }
}
