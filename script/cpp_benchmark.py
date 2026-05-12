#!/usr/bin/env python3
"""Build and run C++ benchmarks for ESPHome components using Google Benchmark."""

import argparse
from functools import partial
import json
import os
from pathlib import Path
import sys

from build_helpers import (
    PLATFORMIO_GOOGLE_BENCHMARK_LIB,
    build_and_run,
    load_test_manifest_overrides,
)
from helpers import root_path

# Path to /tests/benchmarks/components
BENCHMARKS_DIR: Path = Path(root_path) / "tests" / "benchmarks" / "components"

# Path to /tests/benchmarks/core (always included, not a component)
CORE_BENCHMARKS_DIR: Path = Path(root_path) / "tests" / "benchmarks" / "core"

# Stub headers for ESP32-only components (e.g. bluetooth_proxy) that
# allow benchmarks to compile on the host platform.
STUBS_DIR: Path = Path(root_path) / "tests" / "benchmarks" / "stubs"

PLATFORMIO_OPTIONS = {
    "build_flags": [
        "-Os",  # match firmware optimization level (detects inlining regressions)
        "-g",  # debug symbols for profiling
        "-ffunction-sections",  # required for dead-code stripping with -Os
        "-fdata-sections",  # required for dead-code stripping with -Os
        "-DUSE_BENCHMARK",  # disable WarnIfComponentBlockingGuard in finish()
        f"-I{STUBS_DIR}",  # stub headers for ESP32-only components
    ],
    # Use deep+ LDF mode to ensure PlatformIO detects the benchmark
    # library dependency from nested includes.
    "lib_ldf_mode": "deep+",
}


def run_benchmarks(selected_components: list[str], build_only: bool = False) -> int:
    # Allow CI to override the benchmark library (e.g. with CodSpeed's fork).
    # BENCHMARK_LIB_CONFIG is a JSON string from setup_codspeed_lib.py
    # containing {"lib_path": "/path/to/google_benchmark"}.
    lib_config_json = os.environ.get("BENCHMARK_LIB_CONFIG")

    pio_options = PLATFORMIO_OPTIONS
    if lib_config_json:
        lib_config = json.loads(lib_config_json)
        benchmark_lib = f"benchmark=symlink://{lib_config['lib_path']}"
        # These defines must be global (not just in library.json) because
        # benchmark.h uses #ifdef CODSPEED_ENABLED to switch benchmark
        # registration to CodSpeed-instrumented variants, and
        # CODSPEED_ROOT_DIR is used to display relative file paths in reports.
        project_root = Path(__file__).resolve().parent.parent
        codspeed_flags = [
            "-DNDEBUG",
            "-DCODSPEED_ENABLED",
            "-DCODSPEED_ANALYSIS",
            f'-DCODSPEED_ROOT_DIR=\\"{project_root}\\"',
        ]
        pio_options = {
            **PLATFORMIO_OPTIONS,
            "build_flags": PLATFORMIO_OPTIONS["build_flags"] + codspeed_flags,
        }
    else:
        benchmark_lib = PLATFORMIO_GOOGLE_BENCHMARK_LIB

    return build_and_run(
        selected_components=selected_components,
        tests_dir=BENCHMARKS_DIR,
        manifest_override_loader=partial(
            load_test_manifest_overrides, tests_dir=BENCHMARKS_DIR
        ),
        config_prefix="cppbench",
        friendly_name="CPP Benchmarks",
        libraries=benchmark_lib,
        platformio_options=pio_options,
        main_entry="main.cpp",
        label="benchmarks",
        build_only=build_only,
        extra_include_dirs=[CORE_BENCHMARKS_DIR],
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build and run C++ benchmarks for ESPHome components."
    )
    parser.add_argument(
        "components",
        nargs="*",
        help="List of components to benchmark (must have files in tests/benchmarks/components/).",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Benchmark all components with benchmark files.",
    )
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="Only build, print binary path without running.",
    )

    args = parser.parse_args()

    if args.all:
        # Find all component directories that have .cpp files
        components: list[str] = (
            sorted(
                d.name
                for d in BENCHMARKS_DIR.iterdir()
                if d.is_dir()
                and d.name != "__pycache__"
                and (any(d.glob("*.cpp")) or any(d.glob("*.h")))
            )
            if BENCHMARKS_DIR.is_dir()
            else []
        )
    else:
        components: list[str] = args.components

    sys.exit(run_benchmarks(components, build_only=args.build_only))


if __name__ == "__main__":
    main()
