import queue
import unittest

from pickup_analyzer.client import AnalyzerClient, ProtocolError
from pickup_analyzer.transport import Transport


class FakeTransport(Transport):
    def __init__(self):
        self.incoming = queue.Queue()
        self.sent = []

    def send(self, message):
        self.sent.append(message)
        self.incoming.put({
            "type": "response",
            "object": message["object"],
            "action": message["action"],
            "id": message["id"],
            "status": "ok",
        })

    def receive(self):
        return self.incoming.get(timeout=1)

    def close(self):
        pass


class ClientTests(unittest.TestCase):
    def test_routes_asynchronous_acquisition_error_with_zero_id(self):
        transport = FakeTransport()
        client = AnalyzerClient(transport)

        transport.incoming.put({
            "type": "error",
            "id": 0,
            "error": {"code": "invalid_signal", "message": "bad signal"},
        })

        with self.assertRaisesRegex(ProtocolError, "bad signal"):
            client.next_event(timeout=1)

        client.close()

    def test_assigns_ids_and_matches_response(self):
        transport = FakeTransport()
        client = AnalyzerClient(transport)
        response = client.request("device", "info")

        self.assertEqual(response["id"], 1)
        self.assertEqual(transport.sent[0]["type"], "request")
        client.close()

    def test_routes_measurement_event(self):
        transport = FakeTransport()
        client = AnalyzerClient(transport)

        transport.incoming.put({
            "type": "event",
            "object": "measurement",
            "data": {
                "f": 1000,
                "range": 2,
                "rsense": 10000,
                "v": {"re": 0.2, "im": 0},
                "vsense": {"re": 0.04, "im": -0.01},
                "v_min": 1,
                "v_max": 2,
                "vsense_min": 3,
                "vsense_max": 4,
                "z": {"re": 7000, "im": 18000},
            },
        })

        result = client.next_measurement(timeout=1)

        self.assertEqual(result.z.re, 7000)
        client.close()

    def test_profiler_requests(self):
        transport = FakeTransport()
        client = AnalyzerClient(transport)

        self.assertEqual(client.profiler_threads(), [])
        self.assertEqual(client.profiler_data(), [])
        client.reset_profiler()

        self.assertEqual(
            [(message["object"], message["action"]) for message in transport.sent],
            [
                ("profiler", "threads"),
                ("profiler", "data"),
                ("profiler", "reset"),
            ],
        )
        client.close()


if __name__ == "__main__":
    unittest.main()
