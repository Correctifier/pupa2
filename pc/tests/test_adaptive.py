import math
import unittest

from pickup_analyzer.adaptive import (
    AdaptiveSettings,
    LargestErrorMidpointStrategy,
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
    factory=RecursiveMidpointStrategy,
):
    coarse = [
        point(f, model(f))
        for f in logarithmic_frequencies(
            start,
            stop,
            100,
        )
    ]
    strategy = factory(coarse, settings)
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


class LargestErrorTests(unittest.TestCase):
    def test_largest_error_is_reconsidered_after_each_observation(self):
        coarse = [
            point(f, 1j)
            for f in (
                1,
                100,
                10000,
            )
        ]
        errors = iter((
            0.2,
            0.9,
            0.05,
            0.005,
            0.95,
        ))
        strategy = LargestErrorMidpointStrategy(
            coarse,
            AdaptiveSettings(),
            error_metric=lambda *args: next(errors),
        )

        # Bootstrap all initial errors before spending points on descendants.
        for expected in (10, 1000):
            frequency = strategy.next_frequency()

            self.assertAlmostEqual(frequency, expected)
            strategy.observe(point(frequency, 1j))

        # The second interval had the greatest error, so it wins first.
        frequency = strategy.next_frequency()

        self.assertAlmostEqual(frequency, math.sqrt(100 * 1000))
        strategy.observe(point(frequency, 1j))

        # Its sibling still carries 0.9 and beats its new 0.05 descendants.
        frequency = strategy.next_frequency()

        self.assertAlmostEqual(frequency, math.sqrt(1000 * 10000))
        strategy.observe(point(frequency, 1j))

        # The first coarse interval's 0.2 now outranks the remaining 0.05s.
        frequency = strategy.next_frequency()

        self.assertAlmostEqual(frequency, math.sqrt(10))
        strategy.observe(point(frequency, 1j))

        # A newly measured 0.95 moves its children to the front immediately.
        self.assertAlmostEqual(strategy.next_frequency(), math.sqrt(math.sqrt(10)))

    def test_ties_are_deterministic_and_measurements_are_unique(self):
        def run():
            strategy = LargestErrorMidpointStrategy(
                [point(1, 1j), point(100, 1j)],
                AdaptiveSettings(max_points=100),
                error_metric=lambda *args: 1,
            )
            frequencies = []

            while (frequency := strategy.next_frequency()) is not None:
                frequencies.append(frequency)
                strategy.observe(point(frequency, 1j))

            return frequencies, strategy.stop_reason

        frequencies, reason = run()

        self.assertEqual(run(), (frequencies, reason))
        self.assertEqual(reason, "point limit reached")
        self.assertEqual(len(frequencies), 98)
        self.assertEqual(len(set(frequencies)), 98)

    def test_convergence_and_limits(self):
        cases = (
            (
                AdaptiveSettings(),
                lambda f: 1j,
                199,
                "tolerance met",
            ),
            (
                AdaptiveSettings(max_points=150),
                lambda f: 1j,
                150,
                "point limit reached",
            ),
            (
                AdaptiveSettings(tolerance=1e-9, max_depth=1),
                lambda f: complex(f, 0),
                199,
                "refinement limit reached",
            ),
        )

        for settings, model, count, reason in cases:
            with self.subTest(settings=settings):
                points, actual_reason = refine(
                    model,
                    settings,
                    factory=LargestErrorMidpointStrategy,
                )

                self.assertEqual(len(points), count)
                self.assertEqual(actual_reason, reason)

    def test_pending_measurement_contract(self):
        strategy = LargestErrorMidpointStrategy(
            [point(1, 1j), point(100, 1j)],
            AdaptiveSettings(),
        )

        with self.assertRaises(RuntimeError):
            strategy.observe(point(10, 1j))

        frequency = strategy.next_frequency()

        with self.assertRaises(RuntimeError):
            strategy.next_frequency()

        strategy.observe(point(frequency, 1j))
        self.assertIsNone(strategy.next_frequency())


if __name__ == "__main__":
    unittest.main()
