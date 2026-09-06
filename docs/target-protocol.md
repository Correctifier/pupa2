# Target protocol modules

`Application` owns an `Analyzer`, a `Calibration`, and an `ApplicationProtocol`.
The protocol receives the transport, device information, and those two domain
objects. It owns the wire modules and their registration array.

## Request flow

`Router::poll()` reads a line, parses its JSON, and validates the request envelope.
It finds the module whose object name matches the request and calls
`module.process(context, params)`. The module validates its action and parameters,
calls the domain method, and returns the encoded response. The router sends that
response to the requesting endpoint.

There is one dispatch by object name. There are no intermediate request classes,
message variants, visitors, request callback registrations, or typed handler interfaces.
All parameter validation completes before a module invokes domain behavior.

## Where to look

| Location | Responsibility |
| --- | --- |
| `target/source/protocol.*` | Envelope validation, module lookup, and response delivery |
| `target/source/application_protocol.*` | Module ownership and registration |
| `target/source/protocol/module.*` | Module interface, bounded encoding, acknowledgements and errors |
| `target/source/protocol/device.*` | Device information responses and capabilities |
| `target/source/protocol/generator.*` | Generator parameter validation and control |
| `target/source/protocol/sweep.*` | Sweep start/stop requests, event destination, and completion events |
| `target/source/protocol/range.*` | Automatic/manual range requests |
| `target/source/protocol/calibration.*` | Calibration requests |
| `target/source/protocol/measurement.*` | Measurement events and acquisition errors |
| `target/source/analyzer.*` | Generator, range, sweep, acquisition buffer, and measurement processing |
| `target/source/calibration.*` | Calibration placeholder delegating to the BSP |
| `target/source/application.*` | Construct objects and tick the protocol and analyzer |

## Domain interfaces

Generator, sweep, and range modules borrow `Analyzer&`. The calibration module
borrows `Calibration&`. These domain classes contain no protocol headers, JSON,
request identifiers, error codes, or transport endpoints.

`Analyzer` exposes `set_generator()`, `start_sweep()`, `stop_sweep()`,
`set_range_auto()`, `set_range_manual()`, and `tick()`. `Calibration` exposes
`run()`. Modules translate domain outcomes into protocol responses: for example,
`start_sweep()` returning false becomes `busy`, and an unavailable manual range
becomes `invalid_params`.

Valid envelopes retain their ID, object, and action in responses, including
errors. Unknown names are echoed in unsupported-operation errors. Malformed JSON
and invalid envelopes use ID zero and `unknown` names.

## Events

`MeasurementModule` registers its own measurement and invalid-signal callbacks
with `Analyzer` on construction and unregisters them on destruction. This is a
single measurement subscription, independent of sweep starts. `SweepModule`
supplies only a completion callback when it starts a sweep; it has no reference
to the measurement module.

The two modules borrow a shared protocol-owned event destination. A successful
start sets it; rejected busy requests leave it unchanged. Measurement callbacks
send to that destination, and failure clears it. Stop and completion also clear
it. The analyzer knows neither the destination nor any protocol types.

The final measurement is emitted before completion. Stop discards the sweep's
completion callback without emitting completion; the measurement subscription
remains available for subsequent acquisitions. Destroying the sweep module
cancels its active sweep; destroying the measurement module removes its own
subscription. Callback contexts remain valid until removal, and measurement
references are borrowed only for the callback duration.

Callbacks run synchronously from `Analyzer::tick()` and must not reenter or
destroy the analyzer or callback context. There are no added threads, queues,
or allocations. `tick()` returns nothing and there is no publication relay.

## Adding an operation

For an existing object, add action validation and execution to its `process()`
method, calling purpose-specific domain methods. The router does not change.

For a new object, derive a module from `Module`, set its wire name, and override
`process()`. Add the instance to `ApplicationProtocol` and its registration array,
add sources to CMake, and update advertised capabilities if appropriate. No
central message type list or handler overload set needs updating.

Test successful requests, validation before side effects, domain errors, response
identity, and any events. `tests/protocol_tests.cpp` exercises every current
request action through the real application; simulator tests cover acquisition
and sweep completion.

## Ownership and limits

`Application` constructs the domain objects before the protocol. Modules borrow
their domain objects and transport. Device information retains non-owning string
views. The base router borrows the derived protocol's module array, registered
after member construction. Copying and moving remain disabled where internal
references require stable addresses. No dispatch occurs during construction or
destruction.

The context and JSON views remain valid for the synchronous `process()` call.
Modules retain only values needed for future work, such as the event endpoint;
the analyzer copies its sweep parameters. Returned response bytes own their
storage and outlive the parsing document until the transport consumes them.

Messages retain fixed 1,024-byte buffers and `StaticJsonDocument`. Processing
requires no dynamic allocation, exceptions, or RTTI. Encoding rejects overflowing
documents instead of sending partial JSON. Keep new payloads within both their
JSON document capacity and the wire limit.
