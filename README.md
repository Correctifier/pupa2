# Guitar Pickup Impedance Analyzer

An open-source instrument project split into an embedded/virtual C++ target and
a Python desktop client. The current milestone is a usable virtual target.

## Architecture

- `target/source`: hardware-independent application and JSON protocol
- `target/source/protocol`: per-object request handlers and event encoders
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
logic streams newly clean samples through complex demodulation and
four cascaded integrators over the acquisition in `target/source/signal_processing.*` before
forming `Z = Rsense * Vdut / Vsense`.
Target-side control and DSP use single-precision `float` and
`std::complex<float>` to use the STM32G4 hardware FPU efficiently. Desktop-only
plotting and nonlinear fitting retain Python's double-precision arithmetic.
The shared target code follows a heap-free embedded C++ policy documented in
`docs/embedded-cpp.md`; enable `PICKUP_BUILD_STM32_TARGET` only with the
board-specific ARM toolchain and linker inputs.

See [target protocol modules](docs/target-protocol.md) for request dispatch,
module processing, event publishing, and adding new operations.

## Get started

To build and launch both the virtual target and PC app together:

```sh
./scripts/run_virtual.sh
```

The PC app automatically connects to `127.0.0.1:8765`. Closing either app
or pressing Ctrl+C in the terminal stops both. The launcher works from any
directory and forwards arguments to the virtual target, for example
`./scripts/run_virtual.sh --headless` to open only the PC window, or
`./scripts/run_virtual.sh 9000` to use a different port for both apps.
It requires the submodules and build dependencies described below.

To build and run the components separately:

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
Acquisition advances between redraws; the virtual target window redraws at
about 60 FPS without tying each simulated DMA chunk to monitor refresh.

In another terminal, launch the client directly. The TCP client uses only the
Python standard library, so a virtual environment and package installation are
not required:

```sh
./scripts/run_pc.sh
```

Use `./scripts/run_pc.sh --connect` to connect to the virtual target on startup,
or `./scripts/run_pc.sh --connect 127.0.0.1:9000` for a custom TCP endpoint.
Auto-connect retries refused connections for about five seconds while the
target starts. Without `--connect`, connection remains manual.

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

## Adaptive resolution

Select **Adaptive resolution (100 initial points)** in the PC app, then use
**Run once** or **Continuous**. Adaptive mode uses the selected start/stop
frequencies and always begins with 100 log-spaced points; the ordinary Points
field applies only to fixed sweeps. Defaults are **1% tolerance** and **1,000
maximum points**, including the initial grid and every midpoint measurement.

The **Complex midpoint** strategy measures each interval's geometric midpoint,
compares complex impedance with linear interpolation in log-frequency, and
subdivides intervals exceeding the tolerance. It checks intervals breadth-first
so one feature cannot consume the entire budget before the other coarse
intervals are checked. All measured points are retained and plotted in frequency
order. Completed sweep names record whether tolerance was met or a point or
refinement limit was reached; save/load and RLC fitting work as for fixed sweeps.
Each continuous pass starts with a fresh coarse grid.

Even a smooth curve needs 199 measurements to check all 99 initial intervals.
A cap below 199 stops before that first check is complete. The error uses both
real and imaginary impedance, so phase changes matter too. Tolerance describes
the measured midpoint interpolation checks, not a guaranteed bound between
samples; narrow unsampled features can be missed, and measurement noise can
cause extra refinement. Depth and frequency-spacing limits prevent indefinite
subdivision. Update/rebuild the target for the `single_point_sweep` capability.

See [adaptive strategy development](docs/adaptive-sweeps.md) to add another
planner or error metric without changing the GUI worker or firmware.

## C++ editor setup (clangd)

See [coding style](docs/coding-style.md) for multiline argument formatting.

Configure with `cmake -S . -B build` using Ninja or Unix Makefiles. CMake exports
`build/compile_commands.json` by default, and the root `compile_commands.json`
symlink exposes it to clangd for sources and headers throughout the workspace.
Run `cmake --build build -j` to produce any generated headers as well.

In VS Code, install the recommended clangd and CMake Tools extensions. Workspace
settings use `build` and disable the Microsoft C/C++ IntelliSense engine to
avoid duplicate diagnostics. If clangd was running before the first configure,
run **clangd: Restart language server** after configuring.

The clangd arguments allow querying the system GCC drivers under `/usr/bin`
for their standard-library include paths. Other editors should pass the same
`--query-driver` argument from `.vscode/settings.json`; if you select a compiler
elsewhere, add its trusted executable path to that allowlist.

For a different build directory, point the root symlink at that directory's
`compile_commands.json` (for example,
`ln -sfn build-debug/compile_commands.json compile_commands.json`). The selected
database determines the active target and compiler flags; the default desktop
build does not include the unfinished STM32 target.

## STM32 status

The application boundary and STM32 composition-root scaffold are present. The
hardware BSP needs the exact STM32G4 Nucleo-32 part number (for example,
NUCLEO-G431KB) and analog/communications design before adding startup code,
linker scripts, Cube HAL/LL, and peripheral drivers.

## License

MIT
