import math
import unittest

from pickup_analyzer.adaptive import (
    AdaptiveSettings,
    RecursiveMidpointStrategy,
    complex_interpolation_error,
)
from pickup_analyzer.sweep import SweepPoint, logarithmic_frequencies


def point(frequency, value):
    return SweepPoint(
        frequency,
        value.real,
        value.imag,
    )


def refine(
    model,
    settings=AdaptiveSettings(),
    start=20,
    stop=20000,
):
    coarse = [
        point(f, model(f))
        for f in logarithmic_frequencies(
            start,
            stop,
            100,
        )
    ]
    strategy = RecursiveMidpointStrategy(coarse, settings)
    result = coarse.copy()

    while (frequency := strategy.next_frequency()) is not None:
        measured = point(frequency, model(frequency))

        result.append(measured)
        strategy.observe(measured)

    return result, strategy.stop_reason


class AdaptiveTests(unittest.TestCase):
    def test_log_linear_complex_curve_needs_only_first_midpoints(self):
        points, reason = refine(lambda f: complex(100 + math.log(f), 3 * math.log(f)))

        self.assertEqual(len(points), 199)
        self.assertEqual(reason, "tolerance met")
        self.assertEqual(len({p.frequency_hz for p in points}), len(points))

    def test_resonance_gets_more_resolution(self):
        def model(f):
            s = 2j * math.pi * f

            return (7000 + 3 * s) / (1 + 7000 * 120e-12 * s + 3 * 120e-12 * s * s)

        points, reason = refine(model)

        self.assertEqual(reason, "tolerance met")
        self.assertGreater(len(points), 199)

        extras = points[199:]

        self.assertGreater(sum(5000 < p.frequency_hz < 12000 for p in extras), len(extras) / 2)

        # A tighter tolerance should acquire more points on the same noiseless curve.
        tighter, _ = refine(model, AdaptiveSettings(tolerance=0.001))

        self.assertGreater(len(tighter), len(points))

    def test_phase_changes_are_visible_at_constant_magnitude(self):
        left = point(1, 1 + 0j)
        right = point(100, -1 + 0j)
        middle = point(10, 1j)

        self.assertEqual(
            complex_interpolation_error(
                left,
                middle,
                right,
                1e-9,
            ),
            1,
        )

    def test_zero_impedance_is_finite(self):
        self.assertEqual(
            complex_interpolation_error(
                point(1, 0j),
                point(10, 0j),
                point(100, 0j),
                1e-9,
            ),
            0,
        )

        points, reason = refine(lambda f: 0j)

        self.assertEqual((len(points), reason), (199, "tolerance met"))

    def test_budget_includes_coarse_grid_and_midpoints(self):
        for limit in (
            100,
            150,
            200,
        ):
            points, reason = refine(
                lambda f: complex(math.sin(50 * math.log(f)), 1),
                AdaptiveSettings(tolerance=1e-9, max_points=limit),
            )

            self.assertEqual(len(points), limit)
            self.assertEqual(reason, "point limit reached")

    def test_depth_limit_does_not_claim_convergence(self):
        points, reason = refine(
            lambda f: complex(f, 0),
            AdaptiveSettings(tolerance=1e-9, max_depth=1),
        )

        self.assertEqual(len(points), 199)
        self.assertEqual(reason, "refinement limit reached")

    def test_frequency_resolution_limit(self):
        points, reason = refine(
            lambda f: 1j,
            start=100,
            stop=100.00001,
        )

        self.assertEqual(len(points), 100)
        self.assertEqual(reason, "refinement limit reached")

    def test_custom_metric_and_breadth_first_order(self):
        coarse = [
            point(f, 1j)
            for f in (
                1,
                100,
                10000,
            )
        ]
        strategy = RecursiveMidpointStrategy(
            coarse,
            AdaptiveSettings(),
            error_metric=lambda *args: 1,
        )
        first = strategy.next_frequency()

        self.assertAlmostEqual(first, 10)
        strategy.observe(point(first, 1j))
        # Check the other coarse interval before returning to the first's children.
        self.assertAlmostEqual(strategy.next_frequency(), 1000)

    def test_invalid_settings_and_measurements(self):
        for kwargs in (
            {"tolerance": float("nan")},
            {"tolerance": 0},
            {"max_points": 99},
            {"max_points": 100001},
            {"max_depth": 0},
        ):
            with self.assertRaises(ValueError):
                AdaptiveSettings(**kwargs)

        with self.assertRaises(ValueError):
            refine(lambda f: complex(float("nan"), 0))


if __name__ == "__main__":
    unittest.main()
