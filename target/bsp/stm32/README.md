# NUCLEO-G431KB BSP

The board target uses the STM32G431KBT6, ST's G4 HAL v1.2.7, ST's G4 CMSIS
Device v1.2.6, and Arm CMSIS Core 5.9.0 as pinned submodules. The CPU runs at
170 MHz from HSI16 through the PLL, with the Cortex-M4F single-precision FPU.
No external oscillator or board solder-bridge changes are needed for clocks.

## Wiring

| Function | Arduino header | MCU pin | Peripheral |
| --- | --- | --- | --- |
| Vdut input | A0 | PA0 | ADC1 channel 1 |
| Vsense input | A1 | PA1 | ADC2 channel 2 |
| Generator output | A3 | PA4 | DAC1 channel 1 |
| Range select S0 (least significant bit) | D0 | PA10 | GPIO output |
| Range select S1 | D1 | PA9 | GPIO output |
| ST-LINK VCP transmit | A7 (reserved) | PA2 | USART2 TX |
| ST-LINK VCP receive | A2 (reserved) | PA3 | USART2 RX |
| Debug LED LD2 | Keep reserved | PB8 | GPIO output |

These use the lowest compatible numbered analog/digital headers while keeping
ST-LINK VCP, SWD, and the debug LED available. The LED toggles every 500 ms;
peripheral initialization/DMA failures stop execution and hold it on.

The range outputs assume an external four-way analog switch or decoder:
S1:S0 = 00 selects 100 ohm, 01 selects 1 kohm, 10 selects 10 kohm, and 11 selects
100 kohm. They do not switch sense resistors directly. Boot selects 10 kohm.
The resistor values are defined in `nucleo_g431kb.cpp` and must match the
external circuit. Auto-ranging estimates impedance from the completed capture's
channel RMS ratio and applies the closest range at the next frequency setting,
before settling. It does not reacquire the current point.

ADC inputs must be conditioned, single-ended signals biased at approximately
1.65 V, within 0–3.3 V, sharing board ground. The DAC likewise generates a
1.65 V-biased waveform. The analog frontend must supply the differential
Vdut/Vsense signals with the gain assumed by the shared DSP. The Nucleo alone
is not the complete pickup measurement circuit.

## Timing and transport

USART2 runs at 115200 baud, 8 data bits, no parity, one stop bit. Connect the
PC client using Serial and the ST-LINK virtual COM device. An interrupt-driven
2 KiB RX ring collects bytes; the main loop copies complete protocol lines.
Overlength lines and UART overflow/error fragments are discarded through a
newline. Transmission consumes each message synchronously before returning.

TIM6 TRGO drives a circular 32-sample DAC waveform and simultaneous ADC1/ADC2
regular conversions. DMA packs ADC1 in the low halfword and ADC2 in the high
halfword, giving the application's `[Vdut, Vsense, ...]` layout. The caller's
buffer is four-byte aligned and stored in DMA-accessible SRAM. Only a completed,
stopped DMA capture is exposed as clean data. ADC errors invalidate the capture.

Supported generator settings are 1–20000 Hz and greater than zero through
1.5 V peak amplitude. Protocol requests outside these bounds return an error.
The timer rate is quantized to integer prescaler/period values and the BSP
reports that actual ADC rate. Measurement frequency labels retain the requested
frequency; small timer quantization errors and HSI clock tolerance remain.

The filter's 128-frame definition controls both acquisition size and the
pre-acquisition settling duration. There are 32 samples per generated period,
so each capture spans approximately four cycles. At 20 kHz the ADC/DAC rate is
approximately 640 ksample/s. Calibration runs the ADCs' internal single-ended
calibration when idle; this is not analog gain/phase or fixture calibration.

## Build, flash, and verify

Follow the cross-build commands in the root README. The ELF retains debug
symbols and uses `-Og` when built with `-DCMAKE_BUILD_TYPE=Debug`; MinSizeRel minimizes code
size. The linker provides 128 KiB Flash and 22 KiB regular SRAM, reserves 4 KiB
of stack, and leaves the separate 10 KiB CCM bank unused. Application and BSP
state have static storage. C++ allocation/deallocation traps, and `_sbrk`
refuses heap growth.

To flash explicitly with STM32CubeProgrammer and reset the board:

```sh
STM32_Programmer_CLI -c port=SWD \
  -w build-stm32/target/targets/stm32g4/pickup_stm32_target.elf -v -rst
```

On hardware, first check the LED heartbeat and a `device/info` request over VCP.
Then scope A3 at the default 1 kHz / 0.25 V peak setting, verify both conditioned
ADC inputs and range outputs, and measure a known resistor before a pickup.
Cross-build and host tests do not verify physical pin routing, analog settling,
DMA operation on silicon, or measurement accuracy. No board has been flashed or
hardware-tested by this change.

## References

- [ST Nucleo-32 board manual UM2397](https://www.st.com/resource/en/user_manual/um2397-stm32g4-nucleo32-board-mb1430-stmicroelectronics.pdf)
- [STM32G431KB product and datasheet](https://www.st.com/en/microcontrollers-microprocessors/stm32g431kb.html)
- [ST G4 HAL source and documentation](https://github.com/STMicroelectronics/stm32g4xx-hal-driver/tree/v1.2.7)
