#include "acquisition.hpp"

#include <algorithm>
#include <array>

#include "gaussian_detector.hpp"
#include "hal_support.hpp"
#include "profiler.hpp"

using pickup::bsp::stm32::detail::check;

namespace {
ADC_HandleTypeDef adc1{}, adc2{};
DMA_HandleTypeDef adc_dma{};
std::array<std::uint32_t, 512> dma_buffer{};
pickup::bsp::stm32::GaussianDetector detector;
float detector_sample_rate{};
bool acquisition_active{};
volatile bool acquisition_done{};
volatile bool acquisition_error{};

void configure_adc(ADC_HandleTypeDef& adc, std::uint32_t channel) {
  adc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  adc.Init.Resolution = ADC_RESOLUTION_12B;
  adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc.Init.LowPowerAutoWait = DISABLE;
  adc.Init.ContinuousConvMode = DISABLE;
  adc.Init.NbrOfConversion = 1;
  adc.Init.DiscontinuousConvMode = DISABLE;
  adc.Init.ExternalTrigConv = adc.Instance == ADC1 ? ADC_EXTERNALTRIG_T6_TRGO : ADC_SOFTWARE_START;
  adc.Init.ExternalTrigConvEdge =
      adc.Instance == ADC1 ? ADC_EXTERNALTRIGCONVEDGE_RISING : ADC_EXTERNALTRIGCONVEDGE_NONE;
  adc.Init.DMAContinuousRequests = ENABLE;
  adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  adc.Init.OversamplingMode = DISABLE;

  check(HAL_ADC_Init(&adc));

  ADC_ChannelConfTypeDef config{};
  config.Channel = channel;
  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
  config.SingleDiff = ADC_SINGLE_ENDED;
  config.OffsetNumber = ADC_OFFSET_NONE;

  check(HAL_ADC_ConfigChannel(&adc, &config));
  check(HAL_ADCEx_Calibration_Start(&adc, ADC_SINGLE_ENDED));
}
}  // namespace

namespace pickup::bsp::stm32::acquisition {
void initialize() {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_ADC12_CLK_ENABLE();

  GPIO_InitTypeDef gpio{};
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;

  HAL_GPIO_Init(GPIOA, &gpio);

  RCC_PeriphCLKInitTypeDef peripheral_clock{};
  peripheral_clock.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
  peripheral_clock.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;

  check(HAL_RCCEx_PeriphCLKConfig(&peripheral_clock));

  adc1.Instance = ADC1;
  adc2.Instance = ADC2;

  configure_adc(adc1, ADC_CHANNEL_1);
  configure_adc(adc2, ADC_CHANNEL_2);

  ADC_MultiModeTypeDef multimode{};
  multimode.Mode = ADC_DUALMODE_REGSIMULT;
  multimode.DMAAccessMode = ADC_DMAACCESSMODE_12_10_BITS;
  multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;

  check(HAL_ADCEx_MultiModeConfigChannel(&adc1, &multimode));

  adc_dma.Instance = DMA1_Channel2;
  adc_dma.Init.Request = DMA_REQUEST_ADC1;
  adc_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
  adc_dma.Init.PeriphInc = DMA_PINC_DISABLE;
  adc_dma.Init.MemInc = DMA_MINC_ENABLE;
  adc_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  adc_dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  adc_dma.Init.Mode = DMA_CIRCULAR;
  adc_dma.Init.Priority = DMA_PRIORITY_VERY_HIGH;

  check(HAL_DMA_Init(&adc_dma));
  __HAL_LINKDMA(
      &adc1,
      DMA_Handle,
      adc_dma
  );
  HAL_NVIC_SetPriority(
      DMA1_Channel2_IRQn,
      1,
      0
  );
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  HAL_NVIC_SetPriority(
      ADC1_2_IRQn,
      1,
      0
  );
  HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
}

void configure(float frequency_hz, float raw_sample_rate_hz) {
  detector.configure(frequency_hz, raw_sample_rate_hz);
  detector_sample_rate = raw_sample_rate_hz / GaussianDetector::decimation;
}

float sample_rate_hz() {
  return detector_sample_rate;
}

void invalidate() {
  if (acquisition_active) {
    check(HAL_ADCEx_MultiModeStop_DMA(&adc1));

    acquisition_error = true;
    acquisition_done = true;
  }
}

bool start(std::uint16_t* buffer, std::size_t count) {
  if (acquisition_active || buffer == nullptr || count < 2) {
    return false;
  }

  acquisition_done = false;
  acquisition_error = false;
  acquisition_active = true;

  detector.begin();

  check(HAL_ADCEx_MultiModeStart_DMA(
      &adc1,
      dma_buffer.data(),
      dma_buffer.size()
  ));

  return true;
}

std::size_t clean_data_count() {
  return 0;
}

bool finish() {
  if (!acquisition_done) {
    return false;
  }

  if (acquisition_active) {
    check(HAL_ADCEx_MultiModeStop_DMA(&adc1));

    acquisition_active = false;
  }

  return true;
}

bool result(DemodulatedSignals& output) {
  return acquisition_done && !acquisition_error && detector.result(output);
}

bool active() {
  return acquisition_active;
}

bool valid() {
  return acquisition_done && !acquisition_error;
}

void calibrate() {
  if (!acquisition_active) {
    check(HAL_ADCEx_MultiModeStop_DMA(&adc1));
    check(HAL_ADCEx_Calibration_Start(&adc1, ADC_SINGLE_ENDED));
    check(HAL_ADCEx_Calibration_Start(&adc2, ADC_SINGLE_ENDED));
  }
}
}  // namespace pickup::bsp::stm32::acquisition

namespace {
void stop_capture(bool error) {
  LL_ADC_REG_StopConversion(ADC1);
  LL_ADC_REG_StopConversion(ADC2);
  __HAL_DMA_DISABLE(&adc_dma);
  __DMB();

  acquisition_error = error;
  acquisition_done = true;
}

void consume(std::size_t offset) {
  if (acquisition_done) {
    return;
  }

  const auto remaining = __HAL_DMA_GET_COUNTER(&adc_dma);

  if ((remaining > dma_buffer.size() / 2) == (offset == 0)) {
    stop_capture(true);

    return;
  }

  __DMB();

  const bool complete =
      detector.process(std::span(dma_buffer).subspan(offset, dma_buffer.size() / 2));

  if ((__HAL_DMA_GET_COUNTER(&adc_dma) > dma_buffer.size() / 2) == (offset == 0)) {
    stop_capture(true);
  } else if (complete) {
    stop_capture(false);
  }
}
}  // namespace

extern "C" void DMA1_Channel2_IRQHandler() {
  pickup::bsp::stm32::profile::start(pickup::bsp::stm32::ProfileId::adc_dma);

  if (__HAL_DMA_GET_FLAG(&adc_dma, __HAL_DMA_GET_HT_FLAG_INDEX(&adc_dma)) &&
      __HAL_DMA_GET_FLAG(&adc_dma, __HAL_DMA_GET_TC_FLAG_INDEX(&adc_dma))) {
    stop_capture(true);
  }

  HAL_DMA_IRQHandler(&adc_dma);
  pickup::bsp::stm32::profile::stop(pickup::bsp::stm32::ProfileId::adc_dma);
}

extern "C" void ADC1_2_IRQHandler() {
  pickup::bsp::stm32::profile::start(pickup::bsp::stm32::ProfileId::adc);
  HAL_ADC_IRQHandler(&adc1);
  pickup::bsp::stm32::profile::stop(pickup::bsp::stm32::ProfileId::adc);
}

extern "C" void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef*) {
  consume(0);
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef*) {
  consume(dma_buffer.size() / 2);
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef*) {
  stop_capture(true);
}
