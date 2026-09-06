"""Use the same protocol client over TCP or serial."""

import sys

from pickup_analyzer.client import AnalyzerClient
from pickup_analyzer.transport import SerialTransport, TcpTransport


def read_short_sweep(client: AnalyzerClient) -> None:
    print(client.device_info())
    client.start_sweep(
        20.0,
        20_000.0,
        11,
    )

    while True:
        event = client.next_event(timeout=5.0)

        if event.get("object") == "measurement":
            data = event["data"]

            print(f"{data['f']:9.2f} Hz  Z={data['z']['re']:+.2f}{data['z']['im']:+.2f}j Ω")
        elif event.get("object") == "sweep" and event.get("action") == "complete":
            break


if __name__ == "__main__":
    # `tcp` talks to the C++ virtual target; `serial /dev/ttyACM0` uses hardware.
    transport = (
        SerialTransport(sys.argv[2])
        if len(sys.argv) > 2 and sys.argv[1] == "serial"
        else TcpTransport("127.0.0.1", 8765)
    )
    client = AnalyzerClient(transport)

    try:
        read_short_sweep(client)
    finally:
        client.close()
