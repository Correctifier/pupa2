# Embedded C++ policy

Code linked into `pickup_application` is intended for an STM32G4 build with no
heap and no C++ exception or RTTI runtime. The PC BSP is outside this policy.

## Allowed library facilities

- `std::array`, `std::optional`, and `std::complex<float>` have value semantics
  and do not allocate. The DSP uses only basic complex arithmetic.
- `std::string_view` is non-owning. All views passed through the target API must
  remain valid for the duration of the call.
- `<algorithm>`, `<cmath>`, and fixed-width integer types are used normally.

Owning dynamic containers (`std::string`, `std::vector`, `std::deque`, maps,
and similar types), iostreams, exceptions, and RTTI must not be introduced into
the shared target code without an explicit memory/runtime design review.

## Fixed memory

- Incoming and outgoing protocol lines are capped at 1,024 bytes.
- Protocol modules and their registration array are owned by `ApplicationProtocol`;
  the domain-object references and router spans are non-owning. Modules validate
  and execute requests directly; dispatch does not allocate.
- Active sweeps retain a completion callback; measurement callbacks are a separate
  analyzer subscription. Modules remove their callbacks before destruction.
  Callback storage is fixed and no `std::function` is used.
- JSON uses ArduinoJson 6 `StaticJsonDocument`; the submodule is pinned to
  v6.21.6 because ArduinoJson 7's default document allocator uses the heap.
- The fourth-order moving average uses four fixed 32-element circular buffers
  per channel rather than `std::deque`.
- `Analyzer` (owned by `Application`) owns a 256-entry (`512 B`) interleaved ADC buffer plus DSP
  state. Construct the application in static storage, not on an RTOS task stack.

The NUCLEO-G431KB configuration enables `-fno-exceptions`, `-fno-rtti`, function/data
sections, and linker garbage collection. Its linker script uses 128 KiB Flash and
22 KiB DMA-accessible SRAM, reserves at least 4 KiB for stack, and leaves CCM
unused. The build emits a map file and memory usage report. Runtime allocation
traps and `_sbrk` refuses heap growth. See the [board BSP](../target/bsp/stm32/README.md)
for wiring and hardware validation.

## BSP rules

`ReceivedLine` is fixed-capacity. An STM32 transport must accumulate partial
bytes, discard an overlength line through its newline, and never expose storage
that an ISR or DMA can modify during `Application::tick()`. `Transport::send()`
must consume or copy the supplied view before returning.
