import json
import tempfile
import unittest
from pathlib import Path

from pickup_analyzer.sweep import (
    Sweep,
    SweepPoint,
    load_sweeps,
    logarithmic_frequencies,
    save_sweeps,
)


class SweepTests(unittest.TestCase):
    def test_logarithmic_frequencies_include_endpoints(self):
        values = logarithmic_frequencies(
            10.0,
            10000.0,
            4,
        )

        self.assertEqual(
            [round(value) for value in values],
            [
                10,
                100,
                1000,
                10000,
            ],
        )

    def test_invalid_range(self):
        with self.assertRaises(ValueError):
            logarithmic_frequencies(
                1000.0,
                100.0,
                10,
            )

    def test_round_trip(self):
        sweep = Sweep(
            "test",
            [
                SweepPoint(
                    1000.0,
                    10.0,
                    20.0,
                    0.4,
                    0.1,
                )
            ],
        )

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sweep.json"

            save_sweeps(path, [sweep])
            self.assertEqual(load_sweeps(path), [sweep])
            self.assertEqual(json.loads(path.read_text())["version"], 1)

    def test_loads_saved_sweeps_without_voltage_readings(self):
        document = {
            "format": "pickup-analyzer-sweeps",
            "version": 1,
            "sweeps": [
                {
                    "name": "old sweep",
                    "points": [
                        {
                            "frequency_hz": 1000.0,
                            "real_ohm": 10.0,
                            "imaginary_ohm": 20.0,
                        }
                    ],
                }
            ],
        }

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sweep.json"

            path.write_text(json.dumps(document))

            point = load_sweeps(path)[0].points[0]

            self.assertIsNone(point.voltage_v)
            self.assertIsNone(point.sense_voltage_v)


if __name__ == "__main__":
    unittest.main()
