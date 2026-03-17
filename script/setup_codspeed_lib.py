#!/usr/bin/env python3
"""Set up CodSpeed's google_benchmark fork as a PlatformIO library.

CodSpeed requires their codspeed-cpp fork for CPU simulation instrumentation.
This script clones the repo and creates a flat PlatformIO-compatible library
by combining google_benchmark sources and codspeed core sources.

Usage:
    python script/setup_codspeed_lib.py [--output-dir DIR]

Prints JSON to stdout with lib_path and build_flags for cpp_benchmark.py.
Git output goes to stderr.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

# Pin to a specific commit for reproducibility
CODSPEED_CPP_REPO = "https://github.com/CodSpeedHQ/codspeed-cpp.git"
CODSPEED_CPP_SHA = "d6b4111428ae1f1667fec9bff009522378d5d347"

DEFAULT_OUTPUT_DIR = "/tmp/codspeed-cpp"


def setup_codspeed_lib(output_dir: Path) -> None:
    """Clone codspeed-cpp and create a flat PlatformIO library.

    Args:
        output_dir: Directory to clone into
    """
    if not (output_dir / ".git").exists():
        subprocess.run(
            ["git", "clone", CODSPEED_CPP_REPO, str(output_dir)],
            check=True,
            stdout=sys.stderr,
            stderr=sys.stderr,
        )
        # Checkout pinned SHA and init submodules in one pass
        subprocess.run(
            ["git", "-C", str(output_dir), "checkout", CODSPEED_CPP_SHA],
            check=True,
            capture_output=True,
        )
        subprocess.run(
            [
                "git",
                "-C",
                str(output_dir),
                "submodule",
                "update",
                "--init",
                "--recursive",
            ],
            check=True,
            stdout=sys.stderr,
            stderr=sys.stderr,
        )

    benchmark_dir = output_dir / "google_benchmark"
    core_dir = output_dir / "core"
    instrument_hooks_dir = core_dir / "instrument-hooks" / "includes"

    # Read version from core/CMakeLists.txt (needed by walltime.cpp)
    version = "0.0.0"
    cmake_file = core_dir / "CMakeLists.txt"
    if cmake_file.exists():
        for line in cmake_file.read_text().splitlines():
            if line.startswith("set(CODSPEED_VERSION"):
                version = line.split()[1].rstrip(")")
                break

    # PlatformIO doesn't compile .cc files — rename to .cpp
    for cc_file in (benchmark_dir / "src").glob("*.cc"):
        cpp_file = cc_file.with_suffix(".cpp")
        if not cpp_file.exists():
            cc_file.rename(cpp_file)

    # Copy codspeed core sources and headers into google_benchmark/src/
    # so PlatformIO compiles everything as one library.
    # .cpp files get a codspeed_ prefix to avoid name collisions.
    # .h files keep their original names since they're referenced by includes.
    for src_file in (core_dir / "src").glob("*"):
        if src_file.suffix == ".cpp":
            dest = benchmark_dir / "src" / f"codspeed_{src_file.name}"
        elif src_file.suffix == ".h":
            dest = benchmark_dir / "src" / src_file.name
        else:
            continue
        if not dest.exists():
            shutil.copy2(src_file, dest)

    # Copy instrument-hooks C source (provides instrument_hooks_* symbols)
    hooks_c = instrument_hooks_dir.parent / "dist" / "core.c"
    if hooks_c.exists():
        dest = benchmark_dir / "src" / "instrument_hooks.c"
        if not dest.exists():
            shutil.copy2(hooks_c, dest)

    # Resolve the ESPHome project root for CODSPEED_ROOT_DIR
    project_root = Path(__file__).resolve().parent.parent

    # Create library.json
    library_json = {
        "name": "benchmark",
        "version": "0.0.0",
        "build": {
            "flags": [
                f"-I{core_dir / 'include'}",
                f"-I{instrument_hooks_dir}",
                "-DHAVE_STD_REGEX",
                "-DHAVE_STEADY_CLOCK",
                "-DBENCHMARK_STATIC_DEFINE",
                "-DCODSPEED_ENABLED",
                "-DCODSPEED_SIMULATION",
                f'-DCODSPEED_VERSION=\\"{version}\\"',
                f'-DCODSPEED_ROOT_DIR=\\"{project_root}\\"',
                '-DCODSPEED_MODE_DISPLAY=\\"simulation\\"',
            ],
            "includeDir": "include",
        },
    }

    (benchmark_dir / "library.json").write_text(
        json.dumps(library_json, indent=2) + "\n"
    )

    # Output JSON config for cpp_benchmark.py
    result = {
        "lib_path": str(benchmark_dir),
    }
    print(json.dumps(result))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(DEFAULT_OUTPUT_DIR),
        help=f"Directory to clone codspeed-cpp into (default: {DEFAULT_OUTPUT_DIR})",
    )
    args = parser.parse_args()

    setup_codspeed_lib(args.output_dir)


if __name__ == "__main__":
    main()
