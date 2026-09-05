# Guitar Pickup Impedance Analyzer

An open-source instrument project split into an embedded/virtual C++ target and
a Python desktop client. The current milestone is a usable virtual target.

## Architecture

- `target/source`: hardware-independent application and JSON protocol
- `target/source/interfaces`: BSP contracts used by application code
- `target/bsp/pc`: simulated pickup and TCP/pseudo-serial implementations
- `target/bsp/stm32`: home for STM32 implementations
- `target/targets/*`: target-dependent builds and composition-root `main.cpp`
- `pc`: Python/Tkinter client supporting TCP and serial

The pickup model is a series DCR/inductance branch shunted by parasitic
capacitance, with independent Gaussian noise added to the real and imaginary
measurements.

The target BSP exposes float DAC frequency/amplitude control and asynchronous
DMA-style acquisition into a caller-owned interleaved ADC buffer. Application
logic streams newly clean samples through complex demodulation and four
cascaded moving-average stages in `target/source/signal_processing.*` before
forming `Z = Rsense * Vdut / Vsense`.
Target-side control and DSP use single-precision `float` and
`std::complex<float>` to use the STM32G4 hardware FPU efficiently. Desktop-only
plotting and nonlinear fitting retain Python's double-precision arithmetic.

## Get started

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/target/targets/virtual/pickup_virtual_target
```

The virtual target listens on `127.0.0.1:8765` and prints its pseudo-terminal
path at startup. Its ImGui window controls DCR, inductance, parallel parasitic
capacitance, and measurement noise.
Pass `--headless` to run the transport/model without opening a window (useful
for CI), or pass a numeric argument to select a different TCP port. On Linux,
the GUI prefers X11/XWayland for reliable minimize/restore behavior; pass
`--wayland` to explicitly use GLFW's native Wayland backend.

In another terminal, launch the client directly. The TCP client uses only the
Python standard library, so a virtual environment and package installation are
not required:

```sh
./scripts/run_pc.sh
```

Choose TCP and `127.0.0.1:8765`. For the pseudo-terminal, install with
`sudo apt install python3-serial`, choose Serial, and enter the path printed by
the virtual target. Alternatively, developers who already have Python virtual
environment support can install the package with `pip install -e './pc[serial]'`.

On Debian/Ubuntu derivatives, an error mentioning `ensurepip` means the optional
venv support is not installed. If an editable development environment is
desired, install it with `sudo apt install python3-venv`, recreate `.venv`, and
then run the pip command. Do not use pip's `--break-system-packages` option for
this project.

The PC app can acquire logarithmically spaced sweeps once or continuously. It
shows magnitude, phase, real/imaginary, and Nyquist plots. The loaded-sweep list
can independently hide or delete traces using checkbox cells. Every completed
or loaded sweep is automatically fitted to the pickup model
`Z(s)=(R+sL)/(1+sRC+LC*s^2)`. Fitted DCR, inductance, capacitance,
complex-domain R-squared, and residual standard deviation appear in the sweep
table, where a second checkbox toggles the fitted trace overlay. Sweeps can be
saved together as versioned JSON and loaded later.
Magnitude uses logarithmic axes and supports automatic or user-selected
vertical limits. Phase is fixed at -180 to 180 degrees. Nyquist plots maintain
equal ohms-per-pixel scaling on their real and imaginary axes.
The Console tab shows timestamped transmitted and received NDJSON messages and
keeps a bounded 2,000-line history during long or continuous sweeps.
After connecting, the connection block displays target identity, application
name and version, protocol version, and advertised capabilities from the
`device/info` response.
See `pc/examples/analyzer_client_example.py` for the same `AnalyzerClient`
running over either TCP or serial.
Live acquisition is coalesced to the GUI refresh rate, and displayed traces are
pixel-aware decimated while full-resolution samples remain available for saved
files. This keeps multi-thousand-point sweeps responsive.

## STM32 status

The application boundary and STM32 composition-root scaffold are present. The
hardware BSP needs the exact STM32G4 Nucleo-32 part number (for example,
NUCLEO-G431KB) and analog/communications design before adding startup code,
linker scripts, Cube HAL/LL, and peripheral drivers.

## License

MIT
