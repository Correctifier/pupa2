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
logic streams newly clean samples through complex demodulation and four
cascaded 32-sample moving-average stages in `target/source/signal_processing.*` before
forming `Z = Rsense * Vdut / Vsense`. Each sweep point waits one full filter window (128 frames at the actual sample
rate, rounded up to milliseconds) after setting the generator before acquisition.
Acquisition collects 128 frames (256 interleaved ADC entries). Both lengths derive
from the filter definitions in `signal_processing.hpp`; command handling remains responsive.
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
path at startup. Its ImGui window shows the active generator frequency,
amplitude, and range, and controls DCR, inductance, parallel parasitic capacitance,
and measurement noise. Both targets start in **fixed 100 kΩ** range (index 2).
Range indices 0–3 select 1 kΩ, 10 kΩ, 100 kΩ, and 1 MΩ, respectively, as defined
in `target/source/range_selection.hpp`. Auto-ranging is opt-in.
The PC app's Performance tab displays per-context execution counts, exclusive
minimum/average/maximum time, accumulated time, and CPU load. STM32 data comes
from its cycle-counter profiler; the virtual target supplies a synthetic signal
for exercising the monitoring UI.

The [simulator circuit model](docs/simulator.md) retains capacitor voltage,
inductor current, and source phase across generator and range changes, including
during idle and settling time.
Pass `--headless` to run the transport/model without opening a window (useful
for CI), or pass a numeric argument to select a different TCP port. On Linux,
the GUI prefers X11/XWayland for reliable minimize/restore behavior; pass
`--wayland` to explicitly use GLFW's native Wayland backend.
Acquisition advances between redraws; the virtual target window redraws at
about 60 FPS; simulated ADC sampling follows elapsed time independently of redraws.

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
`sudo apt install python3-serial`, choose Serial, and select the port from the
dropdown (or enter the path manually). The path is also printed by
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

The **Largest error first** strategy checks the same initial midpoints, then
uses a priority queue to acquire the candidate with the greatest estimated
error. Each new child midpoint inherits its parent's measured relative complex
interpolation error until it is measured itself. After every acquisition the
new child candidates are ranked against all remaining candidates, so refinement
can switch between features immediately. Equal errors keep insertion order.
The tolerance and point/depth/spacing limits apply to both strategies.

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

Select the active compilation database with either command:

```sh
python scripts/select_clangd.py pc
python scripts/select_clangd.py stm32
```

The selector configures and builds the target (including generated headers), then
switches an ignored local symlink. PC uses `build`; STM32 uses `build-stm32` and
requires the Arm toolchain on `PATH`. Use `--build-dir build-stm32-debug` to select
another build directory; existing build types are preserved. Use separate build
directories for PC and STM32. A failed configure/build leaves the selection intact.

In VS Code, install the recommended clangd and CMake Tools extensions. Run
**Tasks: Run Task → clangd: Use PC target** or **clangd: Use STM32 target**, then
**clangd: Restart language server**. CMake Tools continues to use the PC `build`
directory; these tasks independently select clangd's target.

The root `compile_commands.json` points to the local selection under `.cache`.
Run the selector once after cloning. Workspace settings allow clangd to query
system GCC and Arm GCC installed under `/usr/bin` or STM32CubeCLT under `/opt/st`
for system headers. For another toolchain location, add its trusted executable
path to `--query-driver` in `.vscode/settings.json`. Other editors should use the
same argument; see [clangd system headers](https://clangd.llvm.org/guides/system-headers).

## NUCLEO-G431KB firmware

The STM32 target uses the G4 HAL and CMSIS submodules, an Arm Cortex-M4F
hard-float build, and the onboard ST-LINK virtual COM port at 115200 baud.
See [board wiring and bring-up](target/bsp/stm32/README.md) for pin assignments,
analog requirements, range selection, and firmware validation.

With `arm-none-eabi-gcc` and `arm-none-eabi-g++` on `PATH`:

```sh
git submodule update --init --recursive
cmake -S . -B build-stm32 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-stm32 --parallel
```

ELF, HEX, BIN, and linker-map files are generated under
`build-stm32/target/targets/stm32g4/`. The toolchain file disables host targets
and tests for this build. The shared application is compiled into the firmware.
CI also cross-builds the firmware and uploads these artifacts.

### Run and debug from VS Code

Install the recommended Cortex-Debug extension and `gdb-multiarch`. On Ubuntu or
Pop!_OS:

```sh
sudo apt install gdb-multiarch
```

The Run and Debug panel provides two configurations:

- **STM32: Debug at main** builds and flashes the Debug firmware, then stops at
  `main` with breakpoints and stepping enabled.
- **STM32: Flash and run** builds and flashes the same firmware, then immediately
  continues execution. Cortex-Debug remains attached, so Pause and Stop remain
  available.

Both configurations run the **STM32: Build Debug** task first. That task performs
a fresh CMake configure, so it also works before `build-stm32-debug` exists.
The checked-in paths select STM32CubeCLT 1.19.0 under `/opt/st`; update
`gdbPath`, `armToolchainPath`, `serverpath`, and `stm32cubeprogrammer` in
`.vscode/launch.json` if those tools are installed elsewhere. If multiple
ST-LINK probes are connected, add the desired `serialNumber` to each launch
configuration.


## License

MIT

## Continuous integration

[Host CI](.github/workflows/ci.yml) runs on pushes, pull requests, and manual
workflow dispatches. It initializes submodules recursively, builds the host
application and virtual target in Debug mode, runs CTest and Python unit tests,
and checks formatting with the pinned tools from `pc[format]`. Debug mode keeps
C++ test assertions enabled. The headless TCP smoke test checks device info and
completion of a three-point sweep; it runs by default and can be disabled for a
manual dispatch.

After installing the development tools with `python -m pip install -e './pc[format]'`,
the additional checks can be run locally with:

```sh
python -m unittest discover -s pc/tests -p 'test_*.py'
python -m unittest discover -s scripts -p 'test_*.py'
python scripts/format_code.py --check
python scripts/smoke_virtual.py build/target/targets/virtual/pickup_virtual_target
```

Virtual environments are local development artifacts and are ignored at any
directory depth; no virtual environment is included in the repository.
