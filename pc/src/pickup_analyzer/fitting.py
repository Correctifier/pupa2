from __future__ import annotations

import math
from dataclasses import dataclass

from .sweep import Sweep, SweepPoint


@dataclass(frozen=True)
class RlcFit:
    resistance_ohm: float
    inductance_h: float
    capacitance_f: float
    parallel_resistance_ohm: float
    r_squared: float
    standard_deviation_ohm: float

    def evaluate(self, frequency_hz: float) -> complex:
        s = complex(0.0, 2.0 * math.pi * frequency_hz)
        series_branch = self.resistance_ohm + s * self.inductance_h
        admittance = (
            1.0 / series_branch
            + 1.0 / self.parallel_resistance_ohm
            + s * self.capacitance_f
        )

        return 1.0 / admittance

    def as_sweep(self, source: Sweep) -> Sweep:
        points = []

        for point in source.points:
            value = self.evaluate(point.frequency_hz)

            points.append(
                SweepPoint(
                    point.frequency_hz,
                    value.real,
                    value.imag,
                )
            )

        return Sweep(f"RLC fit: {source.name}", points)


def _solve(matrix: list[list[float]], vector: list[float]) -> list[float]:
    size = len(vector)
    augmented = [row.copy() + [value] for row, value in zip(matrix, vector)]

    for column in range(size):
        pivot = max(range(column, size), key=lambda row: abs(augmented[row][column]))

        if abs(augmented[pivot][column]) < 1e-24:
            raise ValueError("sweep does not contain enough information for an RLC fit")

        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        divisor = augmented[column][column]
        augmented[column] = [value / divisor for value in augmented[column]]

        for row in range(size):
            if row != column:
                factor = augmented[row][column]
                augmented[row] = [
                    value - factor * pivot_value
                    for value, pivot_value in zip(augmented[row], augmented[column])
                ]

    return [augmented[row][-1] for row in range(size)]


def _values(log_parameters: list[float], frequencies: list[float]) -> list[complex]:
    resistance, inductance, capacitance, parallel_resistance = (
        math.exp(value) for value in log_parameters
    )
    fit = RlcFit(
        resistance,
        inductance,
        capacitance,
        parallel_resistance,
        0.0,
        0.0,
    )

    return [fit.evaluate(frequency) for frequency in frequencies]


def _residual_vector(predicted: list[complex], observed: list[complex]) -> list[float]:
    result = []

    for model, actual in zip(predicted, observed):
        difference = (model - actual) / max(abs(actual), 1e-12)

        result.extend((difference.real, difference.imag))

    return result


def fit_rlc(sweep: Sweep) -> RlcFit:
    if len(sweep.points) < 3:
        raise ValueError("at least three sweep points are required")

    frequencies = [point.frequency_hz for point in sweep.points]
    observed = [complex(point.real_ohm, point.imaginary_ohm) for point in sweep.points]
    low = min(sweep.points, key=lambda point: point.frequency_hz)
    resistance = max(1e-6, low.real_ohm)
    omega_low = 2.0 * math.pi * low.frequency_hz
    inductance = max(1e-9, low.imaginary_ohm / omega_low)
    peak = max(sweep.points, key=lambda point: point.magnitude_ohm)
    omega_peak = 2.0 * math.pi * peak.frequency_hz
    capacitance = min(1e-6, max(1e-15, 1.0 / (omega_peak * omega_peak * inductance)))
    parameters = [
        math.log(resistance),
        math.log(inductance),
        math.log(capacitance),
        math.log(max(peak.magnitude_ohm * 2.0, resistance * 10.0)),
    ]
    damping = 1e-3

    predicted = _values(parameters, frequencies)
    residual = _residual_vector(predicted, observed)
    cost = sum(value * value for value in residual)

    for _ in range(80):
        epsilon = 1e-5
        jacobian_columns = []

        for parameter_index in range(4):
            plus, minus = parameters.copy(), parameters.copy()
            plus[parameter_index] += epsilon
            minus[parameter_index] -= epsilon
            plus_values = _residual_vector(_values(plus, frequencies), observed)
            minus_values = _residual_vector(_values(minus, frequencies), observed)

            jacobian_columns.append(
                [(high - low) / (2.0 * epsilon) for high, low in zip(plus_values, minus_values)]
            )

        normal = [
            [
                sum(
                    jacobian_columns[i][row] * jacobian_columns[j][row]
                    for row in range(len(residual))
                )
                for j in range(4)
            ]
            for i in range(4)
        ]
        gradient = [
            sum(jacobian_columns[i][row] * residual[row] for row in range(len(residual)))
            for i in range(4)
        ]

        for index in range(4):
            normal[index][index] += damping * max(normal[index][index], 1.0)

        step = _solve(normal, [-value for value in gradient])
        candidate = [value + delta for value, delta in zip(parameters, step)]

        try:
            candidate_predicted = _values(candidate, frequencies)
        except (OverflowError, ZeroDivisionError):
            damping *= 10.0

            continue

        candidate_residual = _residual_vector(candidate_predicted, observed)
        candidate_cost = sum(value * value for value in candidate_residual)

        if candidate_cost < cost:
            parameters, predicted, residual, cost = (
                candidate,
                candidate_predicted,
                candidate_residual,
                candidate_cost,
            )
            damping = max(1e-12, damping / 3.0)

            if max(abs(value) for value in step) < 1e-9:
                break
        else:
            damping *= 10.0

    resistance, inductance, capacitance, parallel_resistance = (
        math.exp(value) for value in parameters
    )
    absolute_cost = sum(abs(model - actual) ** 2 for model, actual in zip(predicted, observed))
    mean = sum(observed) / len(observed)
    total_sum = sum(abs(value - mean) ** 2 for value in observed)
    r_squared = 1.0 - absolute_cost / total_sum if total_sum > 0 else 1.0
    deviation = math.sqrt(absolute_cost / max(1, 2 * len(observed) - 4))

    return RlcFit(
        resistance,
        inductance,
        capacitance,
        parallel_resistance,
        r_squared,
        deviation,
    )
