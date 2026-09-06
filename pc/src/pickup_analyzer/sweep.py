from __future__ import annotations

import json
import math
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class SweepPoint:
    frequency_hz: float
    real_ohm: float
    imaginary_ohm: float

    @property
    def magnitude_ohm(self) -> float:
        return math.hypot(self.real_ohm, self.imaginary_ohm)

    @property
    def phase_degrees(self) -> float:
        return math.degrees(math.atan2(self.imaginary_ohm, self.real_ohm))


@dataclass
class Sweep:
    name: str
    points: list[SweepPoint]


def logarithmic_frequencies(
    start_hz: float,
    stop_hz: float,
    count: int,
) -> list[float]:
    if not math.isfinite(start_hz) or not math.isfinite(stop_hz) or start_hz <= 0 or stop_hz <= 0:
        raise ValueError("start and stop frequencies must be positive")
    if stop_hz <= start_hz:
        raise ValueError("stop frequency must be greater than start frequency")
    if count < 2:
        raise ValueError("a sweep must contain at least two points")
    start_log = math.log10(start_hz)
    step = (math.log10(stop_hz) - start_log) / (count - 1)
    return [10 ** (start_log + index * step) for index in range(count)]


def save_sweeps(path: str | Path, sweeps: list[Sweep]) -> None:
    document = {
        "format": "pickup-analyzer-sweeps",
        "version": 1,
        "sweeps": [
            {"name": sweep.name, "points": [asdict(point) for point in sweep.points]}
            for sweep in sweeps
        ],
    }
    Path(path).write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def load_sweeps(path: str | Path) -> list[Sweep]:
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    if document.get("format") != "pickup-analyzer-sweeps" or document.get("version") != 1:
        raise ValueError("not a supported pickup analyzer sweep file")
    sweeps = [
        Sweep(str(item["name"]), [SweepPoint(**point) for point in item["points"]])
        for item in document.get("sweeps", [])
    ]
    if not sweeps:
        raise ValueError("the file contains no sweeps")
    return sweeps
