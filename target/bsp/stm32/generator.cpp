#include "generator.hpp"

#include <array>
#include <cmath>
#include <cstdint>

#include "hal_support.hpp"

using pickup::bsp::stm32::detail::check;
using pickup::bsp::stm32::detail::fail;

namespace {
DAC_HandleTypeDef dac{};
DMA_HandleTypeDef dac_dma{};
TIM_HandleTypeDef timer{};
constexpr std::size_t waveform_length = 32;
alignas(4) std::array<std::uint16_t, waveform_length> waveform{};
float sample_rate = 32000.0F;
}  // namespace

namespace pickup::bsp::stm32::generator {
void initialize() {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DAC1_CLK_ENABLE();
  __HAL_RCC_TIM6_CLK_ENABLE();

  GPIO_InitTypeDef gpio{};
  gpio.Pin = GPIO_PIN_4;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;

  HAL_GPIO_Init(GPIOA, &gpio);

  dac_dma.Instance = DMA1_Channel1;
  dac_dma.Init.Request = DMA_REQUEST_DAC1_CHANNEL1;
  dac_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
  dac_dma.Init.PeriphInc = DMA_PINC_DISABLE;
  dac_dma.Init.MemInc = DMA_MINC_ENABLE;
  dac_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dac_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dac_dma.Init.Mode = DMA_CIRCULAR;
  dac_dma.Init.Priority = DMA_PRIORITY_HIGH;

  check(HAL_DMA_Init(&dac_dma));

  dac.Instance = DAC1;

  __HAL_LINKDMA(
      &dac,
      DMA_Handle1,
      dac_dma
  );
  check(HAL_DAC_Init(&dac));

  DAC_ChannelConfTypeDef dac_config{};
  dac_config.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  dac_config.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  dac_config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  dac_config.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
  dac_config.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  dac_config.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;

  check(HAL_DAC_ConfigChannel(
      &dac,
      &dac_config,
      DAC_CHANNEL_1
  ));
  HAL_NVIC_SetPriority(
      DMA1_Channel1_IRQn,
      1,
      0
  );
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  HAL_NVIC_SetPriority(
      TIM6_DAC_IRQn,
      1,
      0
  );
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

bool supports_control(float frequency_hz, float amplitude_v) {
  return std::isfinite(frequency_hz) && std::isfinite(amplitude_v) && frequency_hz >= 1.0F &&
         frequency_hz <= 20000.0F && amplitude_v > 0.0F && amplitude_v <= 1.5F;
}

void stop() {
  if (timer.Instance != nullptr) {
    check(HAL_TIM_Base_Stop(&timer));
    check(HAL_DAC_Stop_DMA(&dac, DAC_CHANNEL_1));
  }
}

void start(float frequency_hz, float amplitude_v) {
  // 32 DAC samples per cycle keeps 20 kHz sweeps below 1 MS/s.
  const float rate = frequency_hz * waveform_length;
  const float timer_clock = static_cast<float>(HAL_RCC_GetPCLK1Freq());
  const auto prescaler = static_cast<std::uint32_t>(std::ceil(timer_clock / rate / 65536.0F));
  const auto period = static_cast<std::uint32_t>(std::lround(timer_clock / rate / prescaler));
  sample_rate = timer_clock / static_cast<float>(prescaler * period);
  timer.Instance = TIM6;
  timer.Init.Prescaler = prescaler - 1;
  timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  timer.Init.Period = period - 1;
  timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  check(HAL_TIM_Base_Init(&timer));

  TIM_MasterConfigTypeDef trigger{};
  trigger.MasterOutputTrigger = TIM_TRGO_UPDATE;
  trigger.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  check(HAL_TIMEx_MasterConfigSynchronization(&timer, &trigger));

  const float amplitude = amplitude_v * 4095.0F / 3.3F;

  for (std::size_t index = 0; index < waveform.size(); ++index) {
    waveform[index] = static_cast<std::uint16_t>(
        std::lround(2048.0F + amplitude * std::cos(6.28318530718F * index / waveform.size()))
    );
  }

  check(HAL_DAC_SetValue(
      &dac,
      DAC_CHANNEL_1,
      DAC_ALIGN_12B_R,
      waveform.back()
  ));
  check(HAL_DAC_Start_DMA(
      &dac,
      DAC_CHANNEL_1,
      reinterpret_cast<std::uint32_t*>(waveform.data()),
      waveform.size(),
      DAC_ALIGN_12B_R
  ));
  __HAL_DMA_DISABLE_IT(&dac_dma, DMA_IT_HT | DMA_IT_TC);
  check(HAL_TIM_Base_Start(&timer));
}

float sample_rate_hz() {
  return sample_rate;
}
}  // namespace pickup::bsp::stm32::generator

extern "C" void DMA1_Channel1_IRQHandler() {
  HAL_DMA_IRQHandler(&dac_dma);
}

extern "C" void TIM6_DAC_IRQHandler() {
  HAL_DAC_IRQHandler(&dac);
}

extern "C" void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef*) {
  fail();
}

extern "C" void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef*) {
  fail();
}
