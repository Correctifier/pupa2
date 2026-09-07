#pragma once

namespace pickup::bsp::stm32::generator {
void initialize();
bool supports_control(float frequency_hz, float amplitude_v);
// Start the fixed-rate DAC stream. Called once during target startup.
void start(float frequency_hz, float amplitude_v);
// Retune the running stream without stopping DMA or resetting its phase.
void set_control(float frequency_hz, float amplitude_v);
// Physical TIM6 trigger rate shared by DAC and ADC, independent of tone frequency.
float sample_rate_hz();
}  // namespace pickup::bsp::stm32::generator
