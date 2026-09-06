#include "ranges.hpp"

#include <cmath>

#include "range_selection.hpp"
#include "stm32g4xx_hal.h"

namespace {
using pickup::sense_resistors_ohm;

std::uint32_t selected_range = pickup::startup_range_index;
std::uint32_t next_range = pickup::startup_range_index;
bool automatic_range = false;

void apply_range(std::uint32_t index) {
  // External four-way analog switch: D0/PA10 = S0, D1/PA9 = S1.
  GPIOA->BSRR = ((index & 1U) ? GPIO_PIN_10 : GPIO_PIN_10 << 16U) |
                ((index & 2U) ? GPIO_PIN_9 : GPIO_PIN_9 << 16U);
  selected_range = index;
}
}  // namespace

namespace pickup::bsp::stm32::ranges {
void initialize() {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  apply_range(selected_range);

  GPIO_InitTypeDef gpio{};
  gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(GPIOA, &gpio);
}

void apply_pending() {
  if (automatic_range) {
    apply_range(next_range);
  }
}

void observe(std::span<const std::uint16_t> samples) {
  if (!automatic_range) {
    return;
  }

  float voltage_power = 0.0F;
  float sense_power = 0.0F;

  for (std::size_t index = 0; index < samples.size(); index += 2) {
    const float voltage = static_cast<float>(samples[index]) - 2048.0F;
    const float sense = static_cast<float>(samples[index + 1]) - 2048.0F;
    voltage_power += voltage * voltage;
    sense_power += sense * sense;
  }

  if (sense_power > 0.0F && voltage_power > 0.0F) {
    const float impedance =
        sense_resistors_ohm[selected_range] * std::sqrt(voltage_power / sense_power);
    float best_error = INFINITY;

    for (std::uint32_t index = 0; index < sense_resistors_ohm.size(); ++index) {
      const float error = std::abs(std::log(sense_resistors_ohm[index] / impedance));

      if (error < best_error) {
        best_error = error;
        next_range = index;
      }
    }
  }
}

void set_auto() {
  automatic_range = true;
  next_range = selected_range;
}

bool set_manual(std::uint32_t index) {
  if (index >= sense_resistors_ohm.size()) {
    return false;
  }

  automatic_range = false;
  next_range = index;

  apply_range(index);

  return true;
}

std::uint32_t index() {
  return selected_range;
}

float resistance_ohm() {
  return sense_resistors_ohm[selected_range];
}
}  // namespace pickup::bsp::stm32::ranges
