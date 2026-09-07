"""Sweep execution shared by the GUI and headless experiments."""

from __future__ import annotations

import math
import queue
import time
from collections.abc import Callable
from threading import Event

from .adaptive import AdaptiveSettings, StrategyFactory, validate_point
from .client import AnalyzerClient
from .sweep import SweepPoint


class SweepCancelled(Exception):
    pass


def measure_sweep(
    client: AnalyzerClient,
    start: float,
    stop: float,
    count: int,
    cancelled: Event,
    on_point: Callable[[SweepPoint], None],
) -> list[SweepPoint]:
    """Consume a complete target operation, including its completion event."""

    if cancelled.is_set():
        raise SweepCancelled

    client.start_sweep(
        start,
        stop,
        count,
    )

    points: list[SweepPoint] = []
    deadline = time.monotonic() + 10

    try:
        while True:
            if cancelled.is_set():
                raise SweepCancelled

            try:
                event = client.next_event(timeout=0.1)
            except queue.Empty:
                if time.monotonic() >= deadline:
                    raise TimeoutError("No measurement received for 10 seconds")

                continue

            if event.get("object") == "sweep" and event.get("action") == "complete":
                if len(points) != count or event.get("data", {}).get("points") != count:
                    raise ValueError("Target completed a sweep with an unexpected point count")

                return points

            if event.get("object") != "measurement":
                continue

            data = event["data"]
            point = SweepPoint(
                float(data["f"]),
                float(data["z"]["re"]),
                float(data["z"]["im"]),
                math.hypot(
                    float(data["v"]["re"]),
                    float(data["v"]["im"]),
                ),
                math.hypot(
                    float(data["vsense"]["re"]),
                    float(data["vsense"]["im"]),
                ),
            )

            validate_point(point)

            if len(points) >= count:
                raise ValueError("Target returned too many measurements")

            fraction = len(points) / (count - 1) if count > 1 else 0
            expected = math.exp(math.log(start) + fraction * (math.log(stop) - math.log(start)))

            if not math.isclose(
                point.frequency_hz,
                expected,
                rel_tol=1e-5,
            ):
                raise ValueError("Target returned a measurement at an unexpected frequency")

            points.append(point)
            on_point(point)

            deadline = time.monotonic() + 10
    except Exception:
        try:
            client.stop_sweep()
            client.discard_events()
        except Exception:
            # Preserve the acquisition error if the connection is already lost.
            pass

        raise


def adaptive_sweep(
    client: AnalyzerClient,
    start: float,
    stop: float,
    settings: AdaptiveSettings,
    factory: StrategyFactory,
    cancelled: Event,
    on_point: Callable[[SweepPoint], None],
) -> tuple[list[SweepPoint], str]:
    """Measure 100 coarse points, then let the supplied strategy choose additions."""

    if "single_point_sweep" not in client.device_info().get("capabilities", []):
        raise ValueError("This target does not support adaptive acquisition; update its firmware")

    points = measure_sweep(
        client,
        start,
        stop,
        100,
        cancelled,
        on_point,
    )
    strategy = factory(points, settings)

    while len(points) < settings.max_points:
        if cancelled.is_set():
            raise SweepCancelled

        frequency = strategy.next_frequency()

        if frequency is None:
            return sorted(points, key=lambda point: point.frequency_hz), strategy.stop_reason

        if not math.isfinite(frequency) or not start <= frequency <= stop:
            raise ValueError("Adaptive strategy requested a frequency outside the sweep")

        if any(
            math.isclose(
                frequency,
                point.frequency_hz,
                rel_tol=1e-7,
            )
            for point in points
        ):
            raise ValueError("Adaptive strategy requested an already measured frequency")

        measured = measure_sweep(
            client,
            frequency,
            frequency,
            1,
            cancelled,
            on_point,
        )[0]

        points.append(measured)
        strategy.observe(measured)

    # Ask once more so a strategy that converged exactly at the cap can report it.
    frequency = strategy.next_frequency()
    reason = strategy.stop_reason if frequency is None else "point limit reached"

    return sorted(points, key=lambda point: point.frequency_hz), reason
