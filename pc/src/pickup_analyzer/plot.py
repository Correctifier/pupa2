from __future__ import annotations

import math
import tkinter as tk
from collections.abc import Callable

from .sweep import Sweep

Series = tuple[str, str, list[tuple[float, float]]]


class SweepPlot(tk.Canvas):
    COLORS = (
        "#4da3ff",
        "#ff9f43",
        "#58d68d",
        "#d980fa",
        "#ff6b6b",
        "#48dbfb",
    )

    def __init__(
        self,
        master,
        title: str,
        x_label: str,
        y_label: str,
        series_builder: Callable[[Sweep, str], list[Series]],
        log_x: bool = False,
        log_y: bool = False,
        y_range: tuple[float, float] | None = None,
        equal_units: bool = False,
    ):
        super().__init__(
            master,
            background="#15181d",
            highlightthickness=0,
            height=430,
        )

        self.title, self.x_label, self.y_label = title, x_label, y_label
        self.series_builder, self.log_x, self.log_y = series_builder, log_x, log_y
        self.y_range, self.equal_units = y_range, equal_units
        self.sweeps: list[Sweep] = []

        self.bind("<Configure>", lambda _event: self.redraw())
        self.bind("<Map>", lambda _event: self.redraw())

    def set_sweeps(self, sweeps: list[Sweep]) -> None:
        self.sweeps = sweeps

        self.redraw()

    def set_y_range(self, value: tuple[float, float] | None) -> None:
        self.y_range = value

        self.redraw()

    @staticmethod
    def _padded(low: float, high: float) -> tuple[float, float]:
        if low == high:
            padding = abs(low) * 0.1 or 1.0
        else:
            padding = (high - low) * 0.06

        return low - padding, high + padding

    def redraw(self) -> None:
        self.delete("all")

        # Notebook tabs that are not visible retain their data and redraw when
        # Tk maps/configures them, avoiding four full renders per live update.
        if not self.winfo_ismapped():
            return

        width, height = self.winfo_width(), self.winfo_height()

        if width < 150 or height < 120:
            return

        left, top, right, bottom = 78, 38, width - 24, height - 58

        self.create_text(
            width / 2,
            17,
            text=self.title,
            fill="#f1f3f5",
            font=(
                "TkDefaultFont",
                11,
                "bold",
            ),
        )
        self.create_text(
            width / 2,
            height - 17,
            text=self.x_label,
            fill="#b9c0c9",
        )
        self.create_text(
            15,
            (top + bottom) / 2,
            text=self.y_label,
            fill="#b9c0c9",
            angle=90,
        )

        series: list[Series] = []
        color_index = 0

        for sweep in self.sweeps:
            built = self.series_builder(sweep, self.COLORS[color_index % len(self.COLORS)])

            series.extend(built)

            color_index += len(built)

        values = [
            (x, y)
            for _, _, points in series
            for x, y in points
            if (x > 0 or not self.log_x) and (y > 0 or not self.log_y)
        ]

        if not values:
            self.create_text(
                width / 2,
                height / 2,
                text="No sweep data",
                fill="#747d8c",
            )

            return

        transformed = [
            (math.log10(x) if self.log_x else x, math.log10(y) if self.log_y else y)
            for x, y in values
        ]
        x_min, x_max = min(x for x, _ in transformed), max(x for x, _ in transformed)

        if self.y_range is None:
            y_min, y_max = self._padded(
                min(y for _, y in transformed),
                max(y for _, y in transformed),
            )
        else:
            y_min = math.log10(self.y_range[0]) if self.log_y else self.y_range[0]
            y_max = math.log10(self.y_range[1]) if self.log_y else self.y_range[1]

        if x_min == x_max:
            x_min, x_max = self._padded(x_min, x_max)

        if self.equal_units:
            plot_width, plot_height = right - left, bottom - top
            units_per_pixel = max((x_max - x_min) / plot_width, (y_max - y_min) / plot_height)
            x_center, y_center = (x_min + x_max) / 2, (y_min + y_max) / 2
            x_half = units_per_pixel * plot_width / 2
            y_half = units_per_pixel * plot_height / 2
            x_min, x_max = x_center - x_half, x_center + x_half
            y_min, y_max = y_center - y_half, y_center + y_half

        def screen(x: float, y: float) -> tuple[float, float]:
            tx = math.log10(x) if self.log_x else x
            ty = math.log10(y) if self.log_y else y

            return (
                left + (tx - x_min) / (x_max - x_min) * (right - left),
                bottom - (ty - y_min) / (y_max - y_min) * (bottom - top),
            )

        for index in range(6):
            fraction = index / 5
            x, y = left + fraction * (right - left), bottom - fraction * (bottom - top)

            self.create_line(
                x,
                top,
                x,
                bottom,
                fill="#2b3038",
            )
            self.create_line(
                left,
                y,
                right,
                y,
                fill="#2b3038",
            )

            xv = (
                10 ** (x_min + fraction * (x_max - x_min))
                if self.log_x
                else x_min + fraction * (x_max - x_min)
            )
            yv_transformed = y_min + fraction * (y_max - y_min)
            yv = 10**yv_transformed if self.log_y else yv_transformed

            self.create_text(
                x,
                bottom + 16,
                text=f"{xv:.3g}",
                fill="#929aa5",
                font=("TkDefaultFont", 8),
            )
            self.create_text(
                left - 8,
                y,
                text=f"{yv:.3g}",
                fill="#929aa5",
                anchor="e",
                font=("TkDefaultFont", 8),
            )

        self.create_rectangle(
            left,
            top,
            right,
            bottom,
            outline="#59616d",
        )

        legend_y = top + 8

        for label, color, points in series:
            valid = [
                screen(x, y)
                for x, y in points
                if (x > 0 or not self.log_x) and (y > 0 or not self.log_y)
            ]
            # Keep moderately dense traces exact. For very large sweeps, cap
            # geometry while preserving the extrema within each screen bucket.
            valid = self._decimate(valid, max(2, int((right - left) * 4)))

            if len(valid) >= 2:
                self.create_line(
                    *[v for point in valid for v in point],
                    fill=color,
                    width=2,
                )

            self.create_line(
                right - 145,
                legend_y,
                right - 125,
                legend_y,
                fill=color,
                width=2,
            )
            self.create_text(
                right - 120,
                legend_y,
                text=label,
                fill="#d7dce2",
                anchor="w",
                font=("TkDefaultFont", 8),
            )

            legend_y += 15

    @staticmethod
    def _decimate(points: list[tuple[float, float]], pixel_width: int) -> list[tuple[float, float]]:
        """Bound canvas work while preserving vertical extrema in each pixel bucket."""

        if len(points) <= pixel_width * 2:
            return points

        result: list[tuple[float, float]] = [points[0]]
        bucket_size = (len(points) - 2) / pixel_width

        for bucket in range(pixel_width):
            start = 1 + int(bucket * bucket_size)
            stop = min(len(points) - 1, 1 + int((bucket + 1) * bucket_size))

            if stop <= start:
                continue

            segment = points[start:stop]
            low_index = min(range(len(segment)), key=lambda index: segment[index][1])
            high_index = max(range(len(segment)), key=lambda index: segment[index][1])

            for index in sorted({low_index, high_index}):
                result.append(segment[index])

        result.append(points[-1])

        return result


def magnitude_series(sweep: Sweep, color: str) -> list[Series]:
    return [(
        sweep.name,
        color,
        [(p.frequency_hz, p.magnitude_ohm) for p in sweep.points],
    )]


def phase_series(sweep: Sweep, color: str) -> list[Series]:
    return [(
        sweep.name,
        color,
        [(p.frequency_hz, p.phase_degrees) for p in sweep.points],
    )]


def components_series(sweep: Sweep, color: str) -> list[Series]:
    alternate = SweepPlot.COLORS[(SweepPlot.COLORS.index(color) + 1) % len(SweepPlot.COLORS)]

    return [
        (
            f"{sweep.name} Re",
            color,
            [(p.frequency_hz, p.real_ohm) for p in sweep.points],
        ),
        (
            f"{sweep.name} Im",
            alternate,
            [(p.frequency_hz, p.imaginary_ohm) for p in sweep.points],
        ),
    ]


def nyquist_series(sweep: Sweep, color: str) -> list[Series]:
    return [(
        sweep.name,
        color,
        [(p.real_ohm, p.imaginary_ohm) for p in sweep.points],
    )]
