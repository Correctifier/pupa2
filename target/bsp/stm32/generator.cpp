#include "generator.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

#include "hal_support.hpp"
#include "nco.hpp"
#include "profiler.hpp"

using pickup::bsp::stm32::detail::check;
using pickup::bsp::stm32::detail::fail;

namespace {
DAC_HandleTypeDef dac{};
DMA_HandleTypeDef dac_dma{};
TIM_HandleTypeDef timer{};
constexpr std::size_t waveform_length = 512;
// 170 MHz / 850 = 200 ksample/s for both DAC and ADC, at every tone frequency.
constexpr std::uint32_t timer_ticks = 850;
pickup::bsp::stm32::Nco oscillator;
alignas(4) std::array<std::uint16_t, waveform_length> waveform{};
float sample_rate = 200000.0F;
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
  const auto clock_multiplier = (RCC->CFGR & RCC_CFGR_PPRE1) == 0 ? 1U : 2U;
  const auto timer_clock = HAL_RCC_GetPCLK1Freq() * clock_multiplier;
  sample_rate = static_cast<float>(timer_clock) / timer_ticks;
  timer.Instance = TIM6;
  timer.Init.Prescaler = 0;
  timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  timer.Init.Period = timer_ticks - 1;
  timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  check(HAL_TIM_Base_Init(&timer));

  TIM_MasterConfigTypeDef trigger{};
  trigger.MasterOutputTrigger = TIM_TRGO_UPDATE;
  trigger.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  check(HAL_TIMEx_MasterConfigSynchronization(&timer, &trigger));

  oscillator.reset(
      frequency_hz,
      sample_rate,
      amplitude_v
  );

  // Prime the DAC pipeline, then enqueue the following samples in time order.
  const auto first_sample = oscillator.next();

  oscillator.fill(waveform);

  check(HAL_DAC_SetValue(
      &dac,
      DAC_CHANNEL_1,
      DAC_ALIGN_12B_R,
      first_sample
  ));
  check(HAL_DAC_Start_DMA(
      &dac,
      DAC_CHANNEL_1,
      reinterpret_cast<std::uint32_t*>(waveform.data()),
      waveform.size(),
      DAC_ALIGN_12B_R
  ));
  check(HAL_TIM_Base_Start(&timer));
}

float sample_rate_hz() {
  return sample_rate;
}
}  // namespace pickup::bsp::stm32::generator

extern "C" void DMA1_Channel1_IRQHandler() {
  pickup::bsp::stm32::profile::start(pickup::bsp::stm32::ProfileId::dac_dma);

  if (__HAL_DMA_GET_FLAG(&dac_dma, __HAL_DMA_GET_HT_FLAG_INDEX(&dac_dma)) &&
      __HAL_DMA_GET_FLAG(&dac_dma, __HAL_DMA_GET_TC_FLAG_INDEX(&dac_dma))) {
    fail();
  }

  HAL_DMA_IRQHandler(&dac_dma);
  pickup::bsp::stm32::profile::stop(pickup::bsp::stm32::ProfileId::dac_dma);
}

extern "C" void TIM6_DAC_IRQHandler() {
  pickup::bsp::stm32::profile::start(pickup::bsp::stm32::ProfileId::timer_dac);
  HAL_DAC_IRQHandler(&dac);
  pickup::bsp::stm32::profile::stop(pickup::bsp::stm32::ProfileId::timer_dac);
}

namespace {
void refill(std::size_t offset) {
  const auto remaining = __HAL_DMA_GET_COUNTER(&dac_dma);
  const bool first_half_active = remaining > waveform_length / 2;

  if (first_half_active == (offset == 0)) {
    fail();
  }

  oscillator.fill(std::span(waveform).subspan(offset, waveform_length / 2));
  __DMB();

  // A missed refill deadline must not silently repeat stale waveform samples.
  if ((__HAL_DMA_GET_COUNTER(&dac_dma) > waveform_length / 2) == (offset == 0)) {
    fail();
  }
}
}  // namespace

extern "C" void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef*) {
  refill(0);
}

extern "C" void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef*) {
  refill(waveform_length / 2);
}

extern "C" void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef*) {
  fail();
}

extern "C" void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef*) {
  fail();
}
