"""Pure adaptive planners: no GUI, transport, threads, or RLC model assumptions."""

from __future__ import annotations

import math
from collections import deque
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from typing import Protocol

from .sweep import SweepPoint


@dataclass(frozen=True)
class AdaptiveSettings:
    tolerance: float = 0.01
    max_points: int = 1000
    max_depth: int = 12
    min_log_span: float = 1e-5
    impedance_floor: float = 1e-9

    def __post_init__(self):
        if not math.isfinite(self.tolerance) or self.tolerance <= 0:
            raise ValueError("Adaptive tolerance must be positive and finite")
        if not isinstance(self.max_points, int) or not 100 <= self.max_points <= 100000:
            raise ValueError("Adaptive maximum points must be between 100 and 100000")
        if not isinstance(self.max_depth, int) or self.max_depth < 1:
            raise ValueError("Adaptive maximum depth must be a positive integer")
        if not math.isfinite(self.min_log_span) or self.min_log_span <= 0:
            raise ValueError("Minimum log-frequency span must be positive and finite")
        if not math.isfinite(self.impedance_floor) or self.impedance_floor <= 0:
            raise ValueError("Impedance floor must be positive and finite")


class AdaptiveStrategy(Protocol):
    """Call next_frequency(), acquire it, then observe() before requesting another.

    Each instance owns one refinement pass following the measured coarse grid.
    Returning None ends the pass; stop_reason describes whether limits intervened.
    """

    stop_reason: str

    def next_frequency(self) -> float | None: ...
    def observe(self, point: SweepPoint) -> None: ...


def validate_point(point: SweepPoint) -> None:
    if (
        not all(
            math.isfinite(value)
            for value in (
                point.frequency_hz,
                point.real_ohm,
                point.imaginary_ohm,
            )
        )
        or point.frequency_hz <= 0
    ):
        raise ValueError("Target returned a non-finite measurement or invalid frequency")


def complex_interpolation_error(
    left: SweepPoint,
    middle: SweepPoint,
    right: SweepPoint,
    floor: float,
) -> float:
    # Use the reported frequency, allowing for target timer/float quantization.
    fraction = math.log(middle.frequency_hz / left.frequency_hz) / math.log(
        right.frequency_hz / left.frequency_hz
    )
    z_left = complex(left.real_ohm, left.imaginary_ohm)
    z_right = complex(right.real_ohm, right.imaginary_ohm)
    measured = complex(middle.real_ohm, middle.imaginary_ohm)
    predicted = z_left + fraction * (z_right - z_left)
    return abs(measured - predicted) / max(abs(measured), floor)


ErrorMetric = Callable[
    [
        SweepPoint,
        SweepPoint,
        SweepPoint,
        float,
    ],
    float,
]


class RecursiveMidpointStrategy:
    """Breadth-first log-midpoint refinement, retaining every measured midpoint."""

    def __init__(
        self,
        coarse: Sequence[SweepPoint],
        settings: AdaptiveSettings,
        error_metric: ErrorMetric = complex_interpolation_error,
    ):
        points = sorted(coarse, key=lambda point: point.frequency_hz)
        if len(points) < 2 or len(points) > settings.max_points:
            raise ValueError("Coarse grid must contain 2..max_points measurements")
        for point in points:
            validate_point(point)
        if any(a.frequency_hz >= b.frequency_hz for a, b in zip(points, points[1:])):
            raise ValueError("Coarse frequencies must be distinct")
        self.settings = settings
        self.error_metric = error_metric
        self._intervals = deque(
            (
                a,
                b,
                1,
            )
            for a, b in zip(points, points[1:])
        )
        self._pending: tuple[SweepPoint, SweepPoint, int] | None = None
        self._count = len(points)
        self._limited = False
        self.stop_reason = ""

    def next_frequency(self) -> float | None:
        if self._pending is not None:
            raise RuntimeError("Observe the outstanding frequency before requesting another")
        while self._intervals:
            if self._count >= self.settings.max_points:
                self.stop_reason = "point limit reached"
                return None
            left, right, depth = self._intervals.popleft()
            span = math.log(right.frequency_hz / left.frequency_hz)
            if depth > self.settings.max_depth or span <= self.settings.min_log_span:
                self._limited = True
                continue
            frequency = math.exp((math.log(left.frequency_hz) + math.log(right.frequency_hz)) / 2)
            if not left.frequency_hz < frequency < right.frequency_hz:
                self._limited = True
                continue
            self._pending = (
                left,
                right,
                depth,
            )
            return frequency
        self.stop_reason = "refinement limit reached" if self._limited else "tolerance met"
        return None

    def observe(self, point: SweepPoint) -> None:
        if self._pending is None:
            raise RuntimeError("No outstanding frequency to observe")
        validate_point(point)
        left, right, depth = self._pending
        self._pending = None
        self._count += 1
        if not left.frequency_hz < point.frequency_hz < right.frequency_hz:
            self._limited = True
            return
        error = self.error_metric(
            left,
            point,
            right,
            self.settings.impedance_floor,
        )
        if not math.isfinite(error) or error < 0:
            raise ValueError("Adaptive error metric must return a finite nonnegative value")
        if error > self.settings.tolerance:
            self._intervals.append((
                left,
                point,
                depth + 1,
            ))
            self._intervals.append((
                point,
                right,
                depth + 1,
            ))


StrategyFactory = Callable[[Sequence[SweepPoint], AdaptiveSettings], AdaptiveStrategy]
# Add a factory here to expose another planner in the GUI without editing acquisition.
STRATEGIES: dict[str, StrategyFactory] = {
    "Complex midpoint": RecursiveMidpointStrategy,
}
