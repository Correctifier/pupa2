# STM32 BSP

Implement the contracts in `target/source/interfaces` here. The analyzer API
accepts DAC frequency/amplitude in Hz/V and caller-owned interleaved `uint16_t`
ADC storage ordered `[Vdut, Vsense, ...]`. Buffer counts and clean-data counts
are scalar entries, not channel frames. `start_acquisition()` returns `false`
and leaves an active transfer untouched when DMA is already running. Clean data
must never include entries DMA can still modify; completed transfers retain the
final clean count until the next accepted start. Application logic
must not include STM32 HAL headers; `target/targets/stm32g4/main.cpp` constructs
the BSP objects and injects them into `pickup::Application`.
