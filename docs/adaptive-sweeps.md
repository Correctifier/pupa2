# Adaptive sweep strategies

The PC app owns adaptation. The target only acquires a logarithmic sweep or one
frequency, keeping the embedded application heap-free and strategy-independent.

## Components

- `pc/src/pickup_analyzer/adaptive.py`: pure planning, settings, error metrics,
  and the `STRATEGIES` registry. No I/O, GUI, threads, or model fitting.
- `pc/src/pickup_analyzer/acquisition.py`: target requests, completion validation,
  cancellation, progress callbacks, and an independent maximum-point guard.
- `pc/src/pickup_analyzer/gui.py`: controls, worker thread, sorted live snapshots,
  and completion status. It reads the strategy choices from the registry.

An adaptive pass first measures a 100-point log grid. The runner constructs the
selected factory with `(coarse_points, settings)`. It repeatedly calls
`next_frequency()`, measures the returned frequency, and passes the actual
`SweepPoint` to `observe()`. `None` ends the pass; `stop_reason` explains why.
There is at most one outstanding requested frequency. Each continuous pass
creates a new strategy instance. Returned points include every coarse and
refinement measurement, sorted by frequency.

## Trying another error metric

Register another factory in `adaptive.py`, for example:

```python
def absolute_complex_error(left, middle, right, floor):
    fraction = math.log(middle.frequency_hz / left.frequency_hz) / math.log(
        right.frequency_hz / left.frequency_hz)
    predicted = complex(left.real_ohm, left.imaginary_ohm) + fraction * (
        complex(right.real_ohm, right.imaginary_ohm)
        - complex(left.real_ohm, left.imaginary_ohm))
    return abs(complex(middle.real_ohm, middle.imaginary_ohm) - predicted)
```

This example produces ohms, so use it from Python with a tolerance in ohms;
the current GUI labels tolerance as a percentage and expects a dimensionless
relative metric. To expose another relative metric in that GUI, add:

```python
STRATEGIES["My relative metric"] = lambda coarse, settings: RecursiveMidpointStrategy(
    coarse, settings, error_metric=my_relative_metric)
```

Metrics must return a finite nonnegative number. The default metric is
`abs(Z_measured - Z_interpolated) / max(abs(Z_measured), 1e-9 ohm)`.
Interpolation uses the actual reported frequency, accommodating target float
rounding. All measured points must be finite with positive frequency.

## Replacing the planner

Implement the `AdaptiveStrategy` protocol and register its factory. The planner
may prioritize intervals differently, use a different curvature metric, or use
all accumulated measurements to decide its next point. Requested frequencies
must be finite, within the original range, and distinct from existing samples.
`observe()` receives the acquired value, not an interpolation or fitted value.
The runner enforces the point cap even if a custom planner does not.

The built-in planner processes a queue of intervals breadth-first. Each
midpoint is kept; failing intervals enqueue both halves. It stops at the
tolerance, point cap, depth 12, or natural-log interval width `1e-5`. These last
two settings and the impedance floor can be changed through `AdaptiveSettings`
for Python experiments. Hitting a limit does not report tolerance convergence.

## Validation

```sh
PYTHONPATH=pc/src python3 -m unittest discover -s pc/tests -v
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

Tests cover smooth complex curves, resonance refinement, phase changes, zero
impedance, caps, subdivision limits, alternative strategies, cancellation, and
the target's single-point and stop/restart behavior. Adaptive acquisition uses
the existing ADC sample count and DSP path without reducing measurement work.
