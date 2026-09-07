"""Exercise the headless host executable through the real TCP client."""

import argparse
import math
from pathlib import Path
import socket
import subprocess
import time

from pickup_analyzer.client import AnalyzerClient
from pickup_analyzer.messages import Measurement
from pickup_analyzer.transport import TcpTransport


def main():
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument("executable", type=Path)

    args = parser.parse_args()

    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))

        port = reservation.getsockname()[1]

    process = subprocess.Popen([
        str(args.executable.resolve()),
        "--headless",
        str(port),
    ])
    client = None

    try:
        deadline = time.monotonic() + 5.0

        while True:
            if process.poll() is not None:
                raise RuntimeError(f"virtual target exited with {process.returncode}")

            try:
                transport = TcpTransport("127.0.0.1", port)

                break
            except OSError:
                if time.monotonic() >= deadline:
                    raise TimeoutError("virtual target did not open its TCP port")

                time.sleep(0.05)

        client = AnalyzerClient(transport)
        info = client.device_info()

        assert info["target_name"] == "PC virtual target"
        assert "profiler" in info["capabilities"]

        contexts = client.profiler_threads()
        statistics = client.profiler_data()

        assert contexts
        assert len(statistics) == len(contexts)
        assert {row["id"] for row in statistics} == {row["id"] for row in contexts}
        assert all(row["cpu_percent"] >= 0 for row in statistics)

        client.reset_profiler()
        client.start_sweep(
            1000,
            2000,
            3,
        )

        frequencies = []
        deadline = time.monotonic() + 5.0

        while True:
            remaining = deadline - time.monotonic()

            if remaining <= 0:
                raise TimeoutError("sweep did not complete")

            event = client.next_event(timeout=remaining)

            if event.get("object") == "measurement":
                measurement = Measurement.from_event(event)

                assert measurement.range_index == 2
                assert measurement.sense_resistor_ohm == 100000
                assert math.isfinite(measurement.z.re)
                assert math.isfinite(measurement.z.im)
                frequencies.append(measurement.frequency_hz)
            elif event.get("object") == "sweep" and event.get("action") == "complete":
                assert event["data"]["points"] == 3

                break

        assert len(frequencies) == 3
        assert frequencies[0] == 1000
        assert frequencies[0] < frequencies[1] < frequencies[2]
        assert frequencies[2] == 2000
        assert process.poll() is None
        print("Headless smoke test passed: device info, profiler, and three-point sweep")
    finally:
        if client is not None:
            client.close()

        process.terminate()

        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


if __name__ == "__main__":
    main()
