#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace pickup::bsp::stm32::acquisition {
void initialize();
void invalidate();
bool start(std::uint16_t* buffer, std::size_t count);
std::size_t clean_data_count();
bool finish();
bool active();
bool valid();
std::span<const std::uint16_t> samples();
void calibrate();
}  // namespace pickup::bsp::stm32::acquisition
