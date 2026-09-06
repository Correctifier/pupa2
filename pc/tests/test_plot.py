import unittest

from pickup_analyzer.plot import SweepPlot


class PlotTests(unittest.TestCase):
    def test_decimation_bounds_geometry_and_keeps_extrema(self):
        points = [(float(index), float(index % 17)) for index in range(10000)]
        reduced = SweepPlot._decimate(points, 500)

        self.assertLessEqual(len(reduced), 1002)
        self.assertEqual(reduced[0], points[0])
        self.assertEqual(reduced[-1], points[-1])
        self.assertEqual(max(y for _, y in reduced), 16.0)


if __name__ == "__main__":
    unittest.main()
