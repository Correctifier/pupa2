#include "serial_transport.hpp"

#include "hal_support.hpp"

using pickup::bsp::stm32::detail::check;

namespace {
UART_HandleTypeDef uart{};
constexpr std::size_t receive_capacity = 2048;
std::array<char, receive_capacity> receive_buffer{};
volatile std::size_t receive_head{}, receive_tail{};
volatile bool receive_overflow{};

}  // namespace

namespace pickup::bsp::stm32 {
void initialize_serial() {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();

  GPIO_InitTypeDef gpio{};
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

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
  // At 115200 baud a byte arrives every 87 us, sooner than a DMA half deadline.
  // Let the short RX handler preempt waveform refill/ADC averaging.
  HAL_NVIC_SetPriority(
      USART2_IRQn,
      0,
      0
  );
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  __HAL_UART_ENABLE_IT(&uart, UART_IT_RXNE);
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
