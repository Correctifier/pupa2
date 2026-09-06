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
- Demodulated samples accumulate into four cascaded complex sums per channel, normalized
  once at acquisition end.
- `Analyzer` (owned by `Application`) owns a 4,096-entry (`8 KiB`) interleaved ADC buffer plus DSP
  state. Construct the application in static storage, not on an RTOS task stack.

An STM32 configuration enables `-fno-exceptions`, `-fno-rtti`, function/data
sections, and linker garbage collection. A board-specific map-file review is
still required once HAL, startup code, and a linker script are added.

## BSP rules

`ReceivedLine` is fixed-capacity. An STM32 transport must accumulate partial
bytes, discard an overlength line through its newline, and never expose storage
that an ISR or DMA can modify during `Application::tick()`. `Transport::send()`
must consume or copy the supplied view before returning.
