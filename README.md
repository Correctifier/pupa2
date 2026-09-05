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
for CI), or pass a numeric argument to select a different TCP port.

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
shows magnitude, phase, real/imaginary, and Nyquist plots. Completed sweeps can
be retained as overlays, saved together as versioned JSON, and loaded later.
Magnitude uses logarithmic axes and supports automatic or user-selected
vertical limits. Phase is fixed at -180 to 180 degrees. Nyquist plots maintain
equal ohms-per-pixel scaling on their real and imaginary axes.

## STM32 status

The application boundary and STM32 composition-root scaffold are present. The
hardware BSP needs the exact STM32G4 Nucleo-32 part number (for example,
NUCLEO-G431KB) and analog/communications design before adding startup code,
linker scripts, Cube HAL/LL, and peripheral drivers.

## License

MIT
