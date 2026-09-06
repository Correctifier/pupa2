# Target protocol modules

`Application` owns an `Analyzer`, a `Calibration`, and an `ApplicationProtocol`.
The protocol is constructed with the transport, device information, and references
to those two behavior objects. It owns the wire modules and registration array.
`Application::tick()` polls requests, advances the analyzer, and publishes any
returned acquisition update.

## Where to look

| Location | Responsibility |
| --- | --- |
| `target/source/protocol.hpp` and `.cpp` | Validate wire envelopes, find modules, and send responses |
| `target/source/application_protocol.*` | Own modules and dispatch the message variant to protocol modules |
| `target/source/application_message.hpp` | Complete `ApplicationMessage` variant of incoming requests |
| `target/source/protocol/*_messages.hpp` | Plain request structs grouped by protocol object |
| `target/source/protocol/module.*` | Decode result, bounded serialization, acknowledgements and errors |
| `target/source/protocol/device.*` | Device request decoding, information responses, and capabilities |
| `target/source/protocol/generator.*` | Generator request decoding and validation |
| `target/source/protocol/sweep.*` | Sweep request decoding and completion events |
| `target/source/protocol/range.*` | Automatic/manual range request decoding |
| `target/source/protocol/calibration.*` | Calibration request decoding |
| `target/source/protocol/measurement.*` | Measurement events and acquisition errors |
| `target/source/analyzer.*` | Generator, range, sweep, acquisition buffer, and measurement processing |
| `target/source/calibration.*` | Calibration placeholder delegating to the BSP |
| `target/source/application.*` | Own and coordinate analyzer, calibration, and protocol |

## Requests and responses

The base router parses each line into a fixed JSON document and checks `type`,
`object`, `action`, and the unsigned `id`. It looks up the object in the module
array and calls `decode()`. A module either returns a validated typed message or
an error; it never calls application behavior.

The six incoming types are `DeviceInfoRequest`, `GeneratorSetRequest`,
`SweepStartRequest`, `SweepStopRequest`, `RangeSetRequest`, and
`CalibrationRunRequest`. `ApplicationMessage` contains exactly these alternatives.
The structs contain ordinary values, with no JSON views or callback state.
`SweepStartRequest` also carries the initiator's endpoint for later events.

`ApplicationProtocol::dispatch_message()` contains the only `std::visit`.
For the concrete message it iterates the module array, calling each module's
virtual `handle()` overload. Base overloads return no result; modules override
only their supported types. The first returned response stops dispatch, including
error responses. If every module declines, the router returns unsupported operation. The generator, sweep, and
range modules borrow `Analyzer&`; the calibration module borrows `Calibration&`.
They invoke purpose-specific methods after decoding succeeds.

`Analyzer` exposes `set_generator()`, `start_sweep()`, `stop_sweep()`,
`set_range_auto()`, `set_range_manual()`, and `tick()`. `Calibration` exposes
`run()`. Neither class includes protocol headers, accepts protocol messages,
returns protocol errors, or knows transport endpoints. `SweepParameters` and
`AcquisitionUpdate` are analyzer-owned value types.

Modules translate domain results into acknowledgements and errors. For example,
`start_sweep()` returning false becomes a `busy` error, and an unavailable manual
range becomes `invalid_params`. The device module encodes the supplied device
information. Wire decoding, dispatch, and error construction remain in the
protocol layer.

Every response goes to the requesting endpoint. Valid envelopes retain their ID,
object, and action even on validation or unsupported-operation errors. Unknown
names are echoed in errors. Malformed JSON and invalid envelopes use ID zero and
`unknown` names. Invalid requests never reach a domain method.

## Events

Measurement, sweep-completion, and acquisition-error messages are outbound-only.
They have no incoming behavior handler and are intentionally outside the
request variant. `Analyzer::tick()` returns an optional `AcquisitionUpdate`, owning
the measurement value and completion state. A final measurement and
completion can share one update. An update without a measurement means invalid
signal; no update means acquisition is still pending or idle.
`ApplicationProtocol::publish()` emits the measurement before completion using
`ApplicationProtocol::measurement_acquired()`,
`sweep_complete()`, and `invalid_signal()` delegate to the existing event modules.
`SweepModule` retains the initiator's endpoint only after a successful start.
A rejected busy request cannot replace it. Stop, completion, and invalid-signal
publication clear it. `ApplicationProtocol::publish()` uses that endpoint to
route analyzer updates. Acquisition errors retain their existing ID-zero format.

These methods serialize and send immediately on the calling thread. Dispatch,
responses, and acquisition progress still run synchronously within `tick()`;
there are no new threads or queues. An ISR or another thread would still need
synchronization or marshalling onto the application thread before using them.

## Adding a request

1. Define a plain request struct in the relevant module's `*_messages.hpp` and
   add it to `ApplicationMessage`.
2. Decode and validate the wire parameters in that module. Return the typed
   value on success or a decode error before application dispatch.
3. Have the module call a purpose-specific analyzer or calibration method, adding
   domain functionality there if needed. Translate domain results into protocol
   responses in the module. Declare a default virtual overload on `Module` and
   override it on the supporting module. The visitor and iteration remain unchanged.
4. For a new object, add its module to `ApplicationProtocol` and its registration
   array, add sources to CMake, and update advertised capabilities as appropriate.
5. Test both decoding and routed behavior, including validation failures, response
   identity, and any asynchronous events. `tests/protocol_tests.cpp` covers all
   six current request alternatives through the real application.

## Ownership and limits

`ApplicationProtocol` passes non-owning analyzer and calibration references to
its modules; the modules also borrow the transport.
Its `DeviceInformation` value retains the existing non-owning string views.
`Application` constructs the behavior objects before the protocol. `Analyzer`
owns the ADC buffer and sweep state and borrows the BSP analyzer, as does the
calibration placeholder. Its base router
borrows the derived object's module array; registration occurs after member
construction. No dispatch happens during construction or destruction. Copying
and moving remain disabled because references point into the owning instances.

The variant owns the decoded values. Dispatch and response encoding complete
before the local decoded value and JSON document are destroyed. The envelope's
string views remain local to wire processing, and the analyzer copies the
sweep values it needs for future ticks. Returned string views must remain valid
until response encoding completes.

All message alternatives have non-throwing value semantics. `std::variant` stores
these values inline; dispatch requires no heap allocation, exceptions, or RTTI.
Messages retain fixed 1,024-byte buffers and `StaticJsonDocument`. Encoding rejects
overflowing documents instead of sending partial JSON. Keep new payloads within
both their document capacity and the wire limit.
