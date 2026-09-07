#pragma once

#include <cstdint>

namespace pickup::bsp::stm32::ranges {
void initialize();
void set_auto();
bool set_manual(std::uint32_t index);
void apply_pending();
void observe(float voltage_power, float sense_power);
std::uint32_t index();
float resistance_ohm();
}  // namespace pickup::bsp::stm32::ranges
