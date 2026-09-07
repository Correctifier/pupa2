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
S1:S0 = 00 selects 1 kohm, 01 selects 10 kohm, 10 selects 100 kohm, and 11 selects
1 Mohm. They do not switch sense resistors directly. Boot selects **fixed 100 kohm**;
auto-ranging requires an explicit `range/set` request with mode `auto`.
The shared `target/source/range_selection.hpp` defines the protocol indices,
resistor values, and startup selection for both hardware and simulator. These
values must match the external circuit. Until switching hardware is installed,
use a physical 100 kohm sense resistor and leave the range fixed at index 2.
Auto-ranging estimates impedance from the completed capture's
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
newline. The short UART RX handler can preempt DAC/ADC block processing to meet
the **87-microsecond** byte deadline. Transmission consumes each message synchronously
before returning.

TIM6 TRGO drives the DAC and simultaneous ADC1/ADC2 conversions at a fixed
**400 ksample/s** (170 MHz / 425), independent of generator frequency. A 32-bit
NCO sets the tone frequency using a phase increment calculated from the actual
timer rate. Its interpolated 1024-entry Q15 cosine table lives in Flash; DMA
half/full callbacks refill a circular 512-sample DAC buffer using integer math.
Phase continues across refills and frequency/amplitude changes; retuning takes
effect as the already-buffered samples drain without stopping the DAC stream.
Each half gives **0.64 milliseconds** to refill. DAC underruns and
missed refill deadlines stop execution through the BSP fault handler.

Supported generator settings remain 1–20000 Hz and greater than zero through
1.5 V peak amplitude. Protocol requests outside these bounds return an error.
NCO frequency rounding is at most 0.000024 Hz at the nominal clock rate;
HSI clock tolerance still affects both sample timing and generated frequency.
The fixed update rate keeps DAC reconstruction images near 400 kHz and its
multiples throughout the sweep, making a fixed analog reconstruction low-pass
filter practical. It does not replace that external analog filter.

ADC DMA packs ADC1 in the low halfword and ADC2 in the high halfword of an
internal circular 512-frame buffer. Each completed half is processed in its DMA
callback. Four adjacent frames are boxcar averaged, producing a 100 ksample/s
stream. Each channel is multiplied by a recursive complex oscillator and accumulated
through a Gaussian window spanning five generator cycles. The detector returns one
complex value per channel and retains no capture or FIR-history buffer. Its reported
magnitudes compensate for the known four-sample boxcar droop.

The physical ADC and DAC rates remain equal and fixed at 400 ksample/s. The Gaussian
window is truncated at approximately plus/minus five standard deviations, with a
standard deviation of half a generator cycle. Oscillator normalization occurs once
per DMA block; trigonometric and exponential setup occurs outside the ISR. ADC errors
or missed DMA processing deadlines invalidate the capture. Reported ADC extrema
describe four-sample averages, so they do not reliably detect brief raw-input clipping.
Analog input filtering is still required. Pre-acquisition settling spans four cycles
and includes two milliseconds for the continuously running DAC's queued samples.
Calibration runs the ADCs' internal single-ended calibration when idle; this
is not analog gain/phase or fixture calibration.

## Build, flash, and verify

Follow the cross-build commands in the root README. The ELF retains debug
symbols and uses `-Og` when built with `-DCMAKE_BUILD_TYPE=Debug`; MinSizeRel minimizes code
size. The linker provides 128 KiB Flash and 22 KiB regular SRAM, reserves 8 KiB
of stack, and leaves the separate 10 KiB CCM bank unused. Application and BSP
state have static storage. C++ allocation/deallocation traps, and `_sbrk`
refuses heap growth.

To flash explicitly with STM32CubeProgrammer and reset the board:

```sh
STM32_Programmer_CLI -c port=SWD \
  -w build-stm32/target/targets/stm32g4/pickup_stm32_target.elf -v -rst
```

VS Code also provides **STM32: Debug at main** and **STM32: Flash and run** in
the Run and Debug panel. Both use Cortex-Debug with ST-LINK over SWD and rebuild
`build-stm32-debug` before programming. See the root README for tool paths and
the required host GDB package.

On hardware, first check the LED heartbeat and a `device/info` request over VCP.
Then scope A3 at the default 1 kHz / 0.25 V peak setting and at both frequency
limits, verify that updates stay at 400 ksample/s and DMA refills meet their
deadlines during simultaneous capture and VCP traffic, verify both conditioned
ADC inputs and range outputs, and measure a known resistor before a pickup.
Cross-build and host tests do not verify physical pin routing, analog settling,
DMA operation on silicon, or measurement accuracy. No board has been flashed or
hardware-tested by this change.

## BSP organization

`nucleo_g431kb.hpp` is the public umbrella header. Implementation is split by
peripheral ownership:

- `clock.cpp`: HAL initialization, system clock setup, early clock failure handling, and SysTick IRQ.
- `board.cpp`: board initialization, LED heartbeat, idle wait, and fatal HAL error handling.
- `serial_transport.cpp`: ST-LINK UART transport, receive buffer, and UART IRQ.
- `generator.cpp`: fixed-rate timer, DAC DMA, and its IRQ/callbacks.
- `nco.cpp`: hardware-independent phase accumulator and interpolated waveform generation.
- `gaussian_detector.cpp`: streaming decimation, complex mixing, and Gaussian accumulation.
- `acquisition.cpp`: dual ADC capture, calibration, ADC DMA, and its IRQ/callbacks.
- `adc_decimator.hpp`: streaming ADC pair averaging and effective sample-rate selection.
- `ranges.cpp`: range GPIO, resistor selection, and autorange decisions.
- `frontend.cpp`: coordinates generator, acquisition, and ranges for the analyzer.
- `profiler.cpp`: DWT cycle timing, nested-context accounting, and performance snapshots.

Peripheral handles and interrupt state stay private to their owning source file.
`hal_support.hpp` shares only HAL checking helpers.

The profiler instruments the main loop, USART2, DAC DMA, ADC DMA, ADC, TIM6/DAC,
and SysTick handlers. Reported execution times exclude time spent in nested,
higher-priority interrupts. Idle `WFI` time is intentionally unassigned, so the
sum of context loads is the measured busy CPU load. The PC app's Performance tab
polls the table and statistics once per second and can reset the measurement
window. The virtual target publishes deterministic, time-varying synthetic data
through the same interface and protocol.

## References

- [ST Nucleo-32 board manual UM2397](https://www.st.com/resource/en/user_manual/um2397-stm32g4-nucleo32-board-mb1430-stmicroelectronics.pdf)
- [STM32G431KB product and datasheet](https://www.st.com/en/microcontrollers-microprocessors/stm32g431kb.html)
- [ST G4 HAL source and documentation](https://github.com/STMicroelectronics/stm32g4xx-hal-driver/tree/v1.2.7)
