#pragma once

#include <cstdint>
#include <span>

namespace pickup::bsp::stm32::ranges {
void initialize();
void set_auto();
bool set_manual(std::uint32_t index);
void apply_pending();
void observe(std::span<const std::uint16_t> samples);
std::uint32_t index();
float resistance_ohm();
}  // namespace pickup::bsp::stm32::ranges
