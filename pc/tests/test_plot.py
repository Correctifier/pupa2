import unittest

from pickup_analyzer.plot import SweepPlot, voltage_series
from pickup_analyzer.sweep import Sweep, SweepPoint


class PlotTests(unittest.TestCase):
    def test_voltage_series_plots_both_channels(self):
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

        series = voltage_series(sweep, SweepPlot.COLORS[0])

        self.assertEqual(series[0][0], "test Vdut")
        self.assertEqual(series[0][2], [(1000.0, 0.4)])
        self.assertEqual(series[1][0], "test Vsense")
        self.assertEqual(series[1][2], [(1000.0, 0.1)])

    def test_voltage_series_ignores_legacy_points(self):
        sweep = Sweep("old", [SweepPoint(1000.0, 10.0, 20.0)])

        self.assertEqual(voltage_series(sweep, SweepPlot.COLORS[0]), [])

    def test_decimation_bounds_geometry_and_keeps_extrema(self):
        points = [(float(index), float(index % 17)) for index in range(10000)]
        reduced = SweepPlot._decimate(points, 500)

        self.assertLessEqual(len(reduced), 1002)
        self.assertEqual(reduced[0], points[0])
        self.assertEqual(reduced[-1], points[-1])
        self.assertEqual(max(y for _, y in reduced), 16.0)


if __name__ == "__main__":
    unittest.main()
