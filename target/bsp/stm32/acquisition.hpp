#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "interfaces/impedance_analyzer.hpp"

namespace pickup::bsp::stm32::acquisition {
void initialize();
void invalidate();
// Set while capture is stopped; detector processing uses fixed 4:1 decimation.
void configure(float frequency_hz, float raw_sample_rate_hz);
float sample_rate_hz();
bool start(std::uint16_t* buffer, std::size_t count);
std::size_t clean_data_count();
bool finish();
bool result(DemodulatedSignals& output);
bool active();
bool valid();
void calibrate();
}  // namespace pickup::bsp::stm32::acquisition
