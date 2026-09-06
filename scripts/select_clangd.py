"""Configure/build a target and select its compilation database for clangd."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def select_database(database: Path, destination: Path):
    entries = json.loads(database.read_text())

    if not isinstance(entries, list) or not entries:
        raise ValueError(f"Empty or invalid compilation database: {database}")

    destination.parent.mkdir(parents=True, exist_ok=True)

    descriptor, temporary = tempfile.mkstemp(dir=destination.parent)

    os.close(descriptor)

    temporary = Path(temporary)

    temporary.unlink()

    try:
        temporary.symlink_to(database.resolve())
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument("target", choices=("pc", "stm32"))
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="Use a different build directory",
    )

    args = parser.parse_args()
    stm32 = args.target == "stm32"
    build = args.build_dir or ROOT / ("build-stm32" if stm32 else "build")
    build = build.resolve()
    configure = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(build),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-DPICKUP_BUILD_VIRTUAL_TARGET={'OFF' if stm32 else 'ON'}",
        f"-DPICKUP_BUILD_TESTS={'OFF' if stm32 else 'ON'}",
        f"-DPICKUP_BUILD_STM32_TARGET={'ON' if stm32 else 'OFF'}",
    ]

    if stm32:
        configure.append(f"-DCMAKE_TOOLCHAIN_FILE={ROOT / 'cmake/toolchains/arm-none-eabi.cmake'}")

    subprocess.run(configure, check=True)
    subprocess.run(
        [
            "cmake",
            "--build",
            str(build),
            "--parallel",
            "4",
        ],
        check=True,
    )
    select_database(build / "compile_commands.json", ROOT / ".cache/clangd/compile_commands.json")
    print(f"clangd now uses {args.target}: {build}")
    print("In VS Code, run 'clangd: Restart language server' to reload the selection.")


if __name__ == "__main__":
    main()
