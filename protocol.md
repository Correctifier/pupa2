# Pickup Analyzer Protocol v1

The protocol is UTF-8 newline-delimited JSON (NDJSON) over a reliable byte
stream. Each line contains exactly one JSON object and ends with `\n`. Senders
must not emit literal newlines inside an object. Receivers must buffer partial
reads until a newline and may process multiple lines from one read.

## Envelope

Every message has `type` (`request`, `response`, `event`, or `error`) and an
`object` naming its subsystem. Requests also have `action` and an unsigned
transaction `id`; responses and request-associated errors echo all three.
Requests optionally carry `params`, successful responses use `status: "ok"`
and may carry `data`, and errors use `status: "error"` plus:

```json
{"code":"invalid_params","message":"human-readable detail"}
```

Event messages have `data` and may have `action`, but do not have a transaction
ID because they are asynchronous. Unknown envelope fields must be ignored for
forward compatibility. Numbers are JSON numbers; frequency is Hz, voltage is V,
resistance is ohms, and complex values are `{ "re": number, "im": number }`.

## Requests

```json
{"type":"request","object":"device","action":"info","id":1}
{"type":"request","object":"generator","action":"set","id":2,"params":{"frequency":1000.0,"amplitude":0.25}}
{"type":"request","object":"sweep","action":"start","id":3,"params":{"f_start":20.0,"f_stop":20000.0,"points":100}}
{"type":"request","object":"sweep","action":"stop","id":4}
{"type":"request","object":"range","action":"set","id":5,"params":{"mode":"auto"}}
{"type":"request","object":"range","action":"set","id":6,"params":{"mode":"manual","range":2}}
{"type":"request","object":"calibration","action":"run","id":7}
```

Sweeps are logarithmically spaced including both endpoints. A successful start
response only acknowledges the operation; measurements arrive as events. A
second start while active returns `busy`. Stop is idempotent. Range indices are
device-defined and discoverable through future device metadata.

## Responses and events

```json
{"type":"response","object":"sweep","action":"start","id":3,"status":"ok"}
```

`device/info` returns `target_name`, `application_name`,
`application_version`, numeric `protocol_version`, and a string array of
`capabilities`. Hosts should use capabilities for feature discovery and ignore
unknown values.

Each acquired point is authoritative raw complex channel data plus diagnostics.
`z` is provided for convenience and is calculated as `rsense * v / vsense`.

```json
{"type":"event","object":"measurement","data":{"f":1032.4,"range":2,"rsense":10000.0,"v":{"re":0.214,"im":-0.003},"vsense":{"re":0.041,"im":-0.018},"v_min":812,"v_max":3267,"vsense_min":1450,"vsense_max":2710,"z":{"re":42100.0,"im":18300.0}}}
{"type":"event","object":"sweep","action":"complete","data":{"points":100}}
```

## Errors

Defined initial codes are `malformed_json`, `invalid_envelope`,
`invalid_params`, `unsupported_operation`, `busy`, and on the host-only side
`transport_disconnected`. An error tied to a parseable request echoes its ID,
object, and action. If parsing fails before these can be trusted, ID is zero and
object/action are empty. A bad message does not close the transport.

## Layering

The Python GUI calls `AnalyzerClient`, which owns IDs, pending request matching,
and asynchronous events. `TcpTransport` and `SerialTransport` only frame JSON
messages. The C++ application similarly depends only on BSP transport and
measurement interfaces, allowing the virtual and STM32 targets to share all
protocol semantics.
