#include "nucleo_g431kb.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "stm32g4xx_hal.h"

namespace {
UART_HandleTypeDef uart{};
DAC_HandleTypeDef dac{};
ADC_HandleTypeDef adc1{}, adc2{};
DMA_HandleTypeDef dac_dma{}, adc_dma{};
TIM_HandleTypeDef timer{};
constexpr std::size_t waveform_length = 32;
alignas(4) std::array<std::uint16_t, waveform_length> waveform{};
constexpr std::array<float, 4> resistors{
    100.0F,
    1000.0F,
    10000.0F,
    100000.0F
};
std::uint32_t selected_range = 2;
std::uint32_t next_range = 2;
bool automatic_range = true;
float sample_rate = 32000.0F;
std::uint16_t* acquisition_buffer{};
std::size_t acquisition_count{};
bool acquisition_active{};
volatile bool acquisition_done{};
volatile bool acquisition_error{};
constexpr std::size_t receive_capacity = 2048;
std::array<char, receive_capacity> receive_buffer{};
volatile std::size_t receive_head{}, receive_tail{};
volatile bool receive_overflow{};

[[noreturn]] void fail() {
  __disable_irq();
  HAL_GPIO_WritePin(
      GPIOB,
      GPIO_PIN_8,
      GPIO_PIN_SET
  );

  while (true) {
    __WFI();
  }
}

void check(HAL_StatusTypeDef result) {
  if (result != HAL_OK) {
    fail();
  }
}

void apply_range(std::uint32_t index) {
  if (acquisition_active && index != selected_range) {
    check(HAL_ADCEx_MultiModeStop_DMA(&adc1));

    acquisition_error = true;
    acquisition_done = true;
  }

  // External four-way analog switch: D0/PA10 = S0, D1/PA9 = S1.
  GPIOA->BSRR = ((index & 1U) ? GPIO_PIN_10 : GPIO_PIN_10 << 16U) |
                ((index & 2U) ? GPIO_PIN_9 : GPIO_PIN_9 << 16U);
  selected_range = index;
}

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

namespace pickup::bsp::stm32 {
void initialize_board() {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_DAC1_CLK_ENABLE();
  __HAL_RCC_TIM6_CLK_ENABLE();
  __HAL_RCC_ADC12_CLK_ENABLE();

  GPIO_InitTypeDef gpio{};
  gpio.Pin = GPIO_PIN_8;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(GPIOB, &gpio);
  apply_range(selected_range);

  gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;

  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4;
  gpio.Mode = GPIO_MODE_ANALOG;

  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Alternate = GPIO_AF7_USART2;

  HAL_GPIO_Init(GPIOA, &gpio);

  uart.Instance = USART2;
  uart.Init.BaudRate = 115200;
  uart.Init.WordLength = UART_WORDLENGTH_8B;
  uart.Init.StopBits = UART_STOPBITS_1;
  uart.Init.Parity = UART_PARITY_NONE;
  uart.Init.Mode = UART_MODE_TX_RX;
  uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart.Init.OverSampling = UART_OVERSAMPLING_16;

  check(HAL_UART_Init(&uart));
  HAL_NVIC_SetPriority(
      USART2_IRQn,
      2,
      0
  );
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  __HAL_UART_ENABLE_IT(&uart, UART_IT_RXNE);

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
  adc_dma.Init.Mode = DMA_NORMAL;
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

void heartbeat() {
  static std::uint32_t last_toggle{};
  const auto now = HAL_GetTick();

  if (now - last_toggle >= 500) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);

    last_toggle = now;
  }
}

std::optional<ReceivedLine> SerialTransport::receive() {
  if (receive_overflow) {
    const auto interrupts = __get_PRIMASK();

    __disable_irq();

    receive_tail = receive_head;
    receive_overflow = false;

    __set_PRIMASK(interrupts);

    discard_line_ = true;
    line_.size = 0;
  }

  while (receive_tail != receive_head) {
    const char byte = receive_buffer[receive_tail];

    __DMB();

    receive_tail = (receive_tail + 1) % receive_capacity;

    if (byte == '\n') {
      if (discard_line_) {
        discard_line_ = false;
        line_.size = 0;

        continue;
      }

      if (receive_overflow) {
        line_.size = 0;

        return std::nullopt;
      }

      const auto result = line_;
      line_.size = 0;

      return result;
    }

    if (byte == '\r' || discard_line_) {
      continue;
    }

    if (line_.size == line_.text.size()) {
      discard_line_ = true;
      line_.size = 0;

      continue;
    }

    line_.text[line_.size++] = byte;
  }

  return std::nullopt;
}

void SerialTransport::send(std::uint32_t, std::string_view line) {
  check(HAL_UART_Transmit(
      &uart,
      reinterpret_cast<const std::uint8_t*>(line.data()),
      static_cast<std::uint16_t>(line.size()),
      1000
  ));
}

std::uint32_t Frontend::milliseconds() const {
  return HAL_GetTick();
}

bool Frontend::supports_control(float frequency_hz, float amplitude_v) const {
  return std::isfinite(frequency_hz) && std::isfinite(amplitude_v) && frequency_hz >= 1.0F &&
         frequency_hz <= 20000.0F && amplitude_v > 0.0F && amplitude_v <= 1.5F;
}

void Frontend::set_control(float frequency_hz, float amplitude_v) {
  if (!supports_control(frequency_hz, amplitude_v)) {
    fail();
  }

  if (acquisition_active) {
    check(HAL_ADCEx_MultiModeStop_DMA(&adc1));

    acquisition_error = true;
    acquisition_done = true;
  }

  if (timer.Instance != nullptr) {
    check(HAL_TIM_Base_Stop(&timer));
    check(HAL_DAC_Stop_DMA(&dac, DAC_CHANNEL_1));
  }

  if (automatic_range) {
    apply_range(next_range);
  }

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

bool Frontend::start_acquisition(std::uint16_t* buffer, std::size_t count) {
  if (acquisition_active || buffer == nullptr || count < 2 || count % 2 != 0 ||
      reinterpret_cast<std::uintptr_t>(buffer) % 4 != 0 || count / 2 > 65535) {
    return false;
  }

  acquisition_buffer = buffer;
  acquisition_count = count;
  acquisition_done = false;
  acquisition_error = false;
  acquisition_active = true;

  check(HAL_ADCEx_MultiModeStart_DMA(
      &adc1,
      reinterpret_cast<std::uint32_t*>(buffer),
      count / 2
  ));
  __HAL_DMA_DISABLE_IT(&adc_dma, DMA_IT_HT);

  return true;
}

std::size_t Frontend::clean_data_count() const {
  if (!acquisition_done) {
    return 0;
  }

  __DMB();

  if (acquisition_error) {
    std::fill_n(
        acquisition_buffer,
        acquisition_count,
        2048
    );
  }

  return acquisition_count;
}

bool Frontend::acquisition_finished() const {
  if (!acquisition_done) {
    return false;
  }

  if (acquisition_active) {
    check(HAL_ADCEx_MultiModeStop_DMA(&adc1));

    acquisition_active = false;

    if (automatic_range && !acquisition_error) {
      float voltage_power = 0.0F;
      float sense_power = 0.0F;

      for (std::size_t index = 0; index < acquisition_count; index += 2) {
        const float voltage = static_cast<float>(acquisition_buffer[index]) - 2048.0F;
        const float sense = static_cast<float>(acquisition_buffer[index + 1]) - 2048.0F;
        voltage_power += voltage * voltage;
        sense_power += sense * sense;
      }

      if (sense_power > 0.0F && voltage_power > 0.0F) {
        const float impedance = resistors[selected_range] * std::sqrt(voltage_power / sense_power);
        float best_error = INFINITY;

        for (std::uint32_t index = 0; index < resistors.size(); ++index) {
          const float error = std::abs(std::log(resistors[index] / impedance));

          if (error < best_error) {
            best_error = error;
            next_range = index;
          }
        }
      }
    }
  }

  return true;
}

float Frontend::sample_rate_hz() const {
  return sample_rate;
}

void Frontend::set_range_auto() {
  automatic_range = true;
  next_range = selected_range;
}

bool Frontend::set_range_manual(std::uint32_t index) {
  if (index >= resistors.size()) {
    return false;
  }

  automatic_range = false;
  next_range = index;

  apply_range(index);

  return true;
}

std::uint32_t Frontend::range_index() const {
  return selected_range;
}

float Frontend::sense_resistor_ohm() const {
  return resistors[selected_range];
}

void Frontend::calibrate() {
  if (!acquisition_active) {
    check(HAL_ADCEx_MultiModeStop_DMA(&adc1));
    check(HAL_ADCEx_Calibration_Start(&adc1, ADC_SINGLE_ENDED));
    check(HAL_ADCEx_Calibration_Start(&adc2, ADC_SINGLE_ENDED));
  }
}
}  // namespace pickup::bsp::stm32

extern "C" void USART2_IRQHandler() {
  const auto status = uart.Instance->ISR;

  if ((status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) != 0) {
    uart.Instance->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    receive_overflow = true;
  }

  if ((status & USART_ISR_RXNE_RXFNE) != 0) {
    const char byte = static_cast<char>(uart.Instance->RDR);
    const auto next = (receive_head + 1) % receive_capacity;

    if (next == receive_tail) {
      receive_overflow = true;
    } else {
      receive_buffer[receive_head] = byte;

      __DMB();

      receive_head = next;
    }
  }
}

extern "C" void DMA1_Channel1_IRQHandler() {
  HAL_DMA_IRQHandler(&dac_dma);
}

extern "C" void DMA1_Channel2_IRQHandler() {
  HAL_DMA_IRQHandler(&adc_dma);
}

extern "C" void ADC1_2_IRQHandler() {
  HAL_ADC_IRQHandler(&adc1);
}

extern "C" void TIM6_DAC_IRQHandler() {
  HAL_DAC_IRQHandler(&dac);
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef*) {
  LL_ADC_REG_StopConversion(ADC1);
  LL_ADC_REG_StopConversion(ADC2);
  __DMB();

  acquisition_done = true;
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef*) {
  LL_ADC_REG_StopConversion(ADC1);
  LL_ADC_REG_StopConversion(ADC2);
  __HAL_DMA_DISABLE(&adc_dma);

  acquisition_error = true;
  acquisition_done = true;
}

extern "C" void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef*) {
  fail();
}

extern "C" void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef*) {
  fail();
}
