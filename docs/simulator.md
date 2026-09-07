# Time-domain pickup simulator

The virtual target models an ideal voltage source driving a sense resistor in
series with a pickup. The pickup is a series DCR/inductance branch in parallel
with capacitance and an independent loss resistance:

```text
source ── Rsense ──┬── DCR ── L ──┬── ground
                  ├────── C ──────┤
                  └───── Rloss ───┘
```

`target/bsp/pc/pickup_circuit.*` owns the circuit model, separately from the
virtual BSP's clock, ADC quantization/noise, transport, and GUI.

## Dynamics

The persistent states are the capacitor/pickup voltage `v` and inductor current
`i`. For source voltage `u`, they satisfy:

```text
dv/dt = (u - v) / (Rsense * C) - v / (Rloss * C) - i / C
di/dt = (v - DCR * i) / L
Vexciter = u
Vdut = u - Vsense = v
Vsense = u - v
```

Between setting changes the circuit is linear with constant coefficients and
a sinusoidal source. The solver uses the sinusoidal particular solution plus
the exact matrix-exponential transient. Real and complex pole cases are handled
separately, including the repeated-pole limit. This captures the linear circuit's
transients without a WDF or an internal oversampling loop, and avoids explicit
integration instability for small capacitances and widely separated time scales.
The steady-state phasors are only the forced component: they never replace or
reset the stored physical state.

Changing range preserves capacitor voltage and inductor current. Frequency
changes preserve source phase; amplitude changes can introduce a source voltage
step. Editing R/L/C preserves voltage/current as a practical interactive model,
not a model of the mechanical process of changing component values. The physical
circuit evolves during idle, settling, and acquisition intervals. Slow GUI frames
or delayed polling do not shorten those intervals.

## ADC and range behavior

The simulator samples at 64 frames per generated period. Each frame contains
Vexciter then Vsense, quantized to a 12-bit ADC with a 3.3 V span and midpoint at
2048 counts. Noise is additive Gaussian input noise on each channel; the slider
sets its standard deviation as a percentage of generator peak amplitude. It is
independent of instantaneous phase and does not change the circuit state.

DMA fills the requested buffer according to elapsed monotonic time and exposes
the capture as clean only after completion. A generator or range change during
capture invalidates it, producing the application's invalid-signal result
instead of reporting mixed settings. The clock is injectable for deterministic
integration tests without real-time sleeps.

Both targets share range indices 0–3 for 1 kΩ, 10 kΩ, 100 kΩ, and 1 MΩ.
The default is fixed range 2 (100 kΩ). Explicit auto mode uses the previous
capture's RMS channel ratio to choose the closest sense resistance and applies
it on the next generator setting, before settling. It does not use hidden
knowledge of the circuit's analytical impedance.

The simulator window displays the generator frequency, peak amplitude, current
range index/resistance, and fixed/automatic mode. These readouts follow protocol
commands and sweep steps; circuit sliders do not overwrite generator settings.

## Scope and verification

This is an ideal linear RLC model with an ideal range switch. It does not model
switch charge injection/contact bounce, amplifier bandwidth or clipping, DAC
stair steps, hysteresis, or magnetic nonlinearities. Fixed settling intervals
can still be insufficient for a high-Q circuit: the simulator intentionally
retains that residual transient rather than forcing a settled result.

Tests compare settled impedance against an independent analytical reference
across frequencies and ranges, check continuity and passive ring-down, and
compare differently partitioned time advances. BSP tests cover actual elapsed
capture time, steady ADC waveforms, default/fixed/automatic ranges, status
readouts, and capture invalidation. A full sweep test runs with an injected clock.
