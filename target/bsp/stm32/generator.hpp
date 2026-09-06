#pragma once

namespace pickup::bsp::stm32::generator {
void initialize();
bool supports_control(float frequency_hz, float amplitude_v);
void stop();
// Reconfigure and start after stop(), with no acquisition active.
void start(float frequency_hz, float amplitude_v);
// Physical TIM6 trigger rate shared by DAC and ADC, independent of tone frequency.
float sample_rate_hz();
}  // namespace pickup::bsp::stm32::generator
