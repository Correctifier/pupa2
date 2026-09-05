import unittest

from pickup_analyzer.fitting import RlcFit, fit_rlc
from pickup_analyzer.sweep import Sweep, SweepPoint, logarithmic_frequencies


class FittingTests(unittest.TestCase):
    def test_recovers_exact_rlc_model(self):
        expected = RlcFit(7000.0, 3.0, 120e-12, 0.0, 0.0)
        points = []
        for frequency in logarithmic_frequencies(20.0, 20000.0, 101):
            value = expected.evaluate(frequency)
            points.append(SweepPoint(frequency, value.real, value.imag))
        result = fit_rlc(Sweep("exact", points))
        self.assertGreater(result.r_squared, 0.999999)
        self.assertLess(result.standard_deviation_ohm, 0.01)
        self.assertAlmostEqual(result.resistance_ohm, expected.resistance_ohm, places=3)
        self.assertAlmostEqual(result.inductance_h, expected.inductance_h, places=6)
        self.assertAlmostEqual(result.capacitance_f, expected.capacitance_f, places=18)


if __name__ == "__main__":
    unittest.main()
