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
message variants, visitors, callback registrations, or typed handler interfaces.
All parameter validation completes before a module invokes domain behavior.

## Where to look

| Location | Responsibility |
| --- | --- |
| `target/source/protocol.*` | Envelope validation, module lookup, and response delivery |
| `target/source/application_protocol.*` | Module ownership, registration, and acquisition event publication |
| `target/source/protocol/module.*` | Module interface, bounded encoding, acknowledgements and errors |
| `target/source/protocol/device.*` | Device information responses and capabilities |
| `target/source/protocol/generator.*` | Generator parameter validation and control |
| `target/source/protocol/sweep.*` | Sweep start/stop requests, event destination, and completion events |
| `target/source/protocol/range.*` | Automatic/manual range requests |
| `target/source/protocol/calibration.*` | Calibration requests |
| `target/source/protocol/measurement.*` | Measurement events and acquisition errors |
| `target/source/analyzer.*` | Generator, range, sweep, acquisition buffer, and measurement processing |
| `target/source/calibration.*` | Calibration placeholder delegating to the BSP |
| `target/source/application.*` | Construct objects and coordinate polling, acquisition, and publication |

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

`Analyzer::tick()` returns an optional `AcquisitionUpdate` containing a measurement
and completion state. No update means idle or pending acquisition; an update
without a measurement reports invalid signal. The final measurement can carry
completion in the same update.

`Application` passes each update to `ApplicationProtocol::publish()`, which sends
the measurement before sweep completion. `SweepModule` retains the initiator's
endpoint after a successful start. A busy request cannot replace it. Stop,
completion, and invalid-signal publication clear it. Event encoding remains in
the sweep and measurement modules; acquisition errors keep their ID-zero format.

All polling, execution, responses, and publication run synchronously on the
calling thread. There are no added threads or queues. Calls from an ISR or another
thread still require synchronization or marshalling onto the application thread.

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
