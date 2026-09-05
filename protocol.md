# Wire protocol

Each message is one UTF-8 JSON object followed by `\n`. Requests and responses
contain `type`, `direction`, `transaction_id`, and `payload`.

```json
{"type":"measure_impedance","direction":"request","transaction_id":1,"payload":{"frequency_hz":1000.0}}
{"type":"measure_impedance","direction":"response","transaction_id":1,"payload":{"frequency_hz":1000.0,"real_ohm":7210.0,"imaginary_ohm":19040.0}}
```

Supported requests are `get_info` and `measure_impedance`. Errors are response
messages with type `error` and a `payload.message` string. Transaction IDs are
unsigned integers selected by the requester.

