import queue
import threading
import unittest

from pickup_analyzer.acquisition import SweepCancelled, adaptive_sweep, measure_sweep
from pickup_analyzer.adaptive import AdaptiveSettings, RecursiveMidpointStrategy, STRATEGIES
from pickup_analyzer.sweep import logarithmic_frequencies


class FakeClient:
    def __init__(self):
        self.events = queue.Queue()
        self.requests = []
        self.stopped = False

    def device_info(self):
        return {"capabilities": ["single_point_sweep"]}

    def start_sweep(
        self,
        start,
        stop,
        count,
    ):
        self.requests.append((
            start,
            stop,
            count,
        ))

        for f in (
            [start]
            if count == 1
            else logarithmic_frequencies(
                start,
                stop,
                count,
            )
        ):
            self.events.put({"object": "measurement", "data": {"f": f, "z": {"re": 100, "im": 0}}})

        self.events.put({
            "object": "sweep",
            "action": "complete",
            "data": {"points": count},
        })

    def next_event(self, timeout):
        return self.events.get(timeout=timeout)

    def stop_sweep(self):
        self.stopped = True

    def discard_events(self):
        while not self.events.empty():
            self.events.get_nowait()


class AcquisitionTests(unittest.TestCase):
    def test_coarse_then_single_points_and_sorted_output(self):
        client = FakeClient()
        observed = []
        points, reason = adaptive_sweep(
            client,
            20,
            20000,
            AdaptiveSettings(),
            RecursiveMidpointStrategy,
            threading.Event(),
            observed.append,
        )

        self.assertEqual(reason, "tolerance met")
        self.assertEqual(len(points), 199)
        self.assertEqual(
            client.requests[0],
            (
                20,
                20000,
                100,
            ),
        )
        self.assertTrue(all(a == b and n == 1 for a, b, n in client.requests[1:]))
        self.assertEqual(points, sorted(observed, key=lambda p: p.frequency_hz))
        self.assertTrue(client.events.empty())

    def test_registered_largest_error_strategy_runs_through_acquisition(self):
        client = FakeClient()
        observed = []
        points, reason = adaptive_sweep(
            client,
            20,
            20000,
            AdaptiveSettings(),
            STRATEGIES["Largest error first"],
            threading.Event(),
            observed.append,
        )

        self.assertEqual(reason, "tolerance met")
        self.assertEqual(len(points), 199)
        self.assertEqual(len(client.requests), 100)
        self.assertTrue(all(a == b and n == 1 for a, b, n in client.requests[1:]))
        self.assertEqual(points, sorted(observed, key=lambda p: p.frequency_hz))
        self.assertTrue(client.events.empty())

    def test_cancellation_drains_pending_events_and_allows_restart(self):
        client = FakeClient()
        cancelled = threading.Event()

        def cancel(point):
            cancelled.set()

        with self.assertRaises(SweepCancelled):
            measure_sweep(
                client,
                20,
                20000,
                100,
                cancelled,
                cancel,
            )

        self.assertTrue(client.stopped)
        self.assertTrue(client.events.empty())
        cancelled.clear()

        result = measure_sweep(
            client,
            1000,
            1000,
            1,
            cancelled,
            lambda p: None,
        )

        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].frequency_hz, 1000)

    def test_factory_can_replace_the_whole_strategy(self):
        class OneExtra:
            stop_reason = "experiment complete"

            def __init__(self, coarse, settings):
                self.done = False

            def next_frequency(self):
                return None if self.done else 1234

            def observe(self, point):
                self.done = True

        result, reason = adaptive_sweep(
            FakeClient(),
            20,
            20000,
            AdaptiveSettings(),
            OneExtra,
            threading.Event(),
            lambda p: None,
        )

        self.assertEqual((len(result), reason), (101, "experiment complete"))

    def test_missing_capability_fails_before_acquisition(self):
        client = FakeClient()
        client.device_info = lambda: {"capabilities": []}

        with self.assertRaisesRegex(ValueError, "update its firmware"):
            adaptive_sweep(
                client,
                20,
                20000,
                AdaptiveSettings(),
                RecursiveMidpointStrategy,
                threading.Event(),
                lambda p: None,
            )

        self.assertEqual(client.requests, [])

    def test_runner_enforces_budget_even_for_custom_strategy(self):
        class Endless:
            stop_reason = ""

            def __init__(self, coarse, settings):
                self.frequency = 1001

            def next_frequency(self):
                self.frequency += 1

                return self.frequency

            def observe(self, point):
                pass

        result, reason = adaptive_sweep(
            FakeClient(),
            20,
            20000,
            AdaptiveSettings(max_points=103),
            Endless,
            threading.Event(),
            lambda p: None,
        )

        self.assertEqual((len(result), reason), (103, "point limit reached"))


if __name__ == "__main__":
    unittest.main()
