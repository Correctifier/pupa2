#pragma once

#include "stm32g4xx_hal.h"

namespace pickup::bsp::stm32::detail {
[[noreturn]] void fail();
void check(HAL_StatusTypeDef result);
}  // namespace pickup::bsp::stm32::detail
