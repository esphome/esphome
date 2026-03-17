#!/usr/bin/env python3
"""Build and run C++ benchmarks for ESPHome components using Google Benchmark."""

import argparse
from pathlib import Path
import sys

from helpers import root_path
from test_helpers import PLATFORMIO_GOOGLE_BENCHMARK_LIB, build_and_run

# Path to /tests/benchmarks/components
BENCHMARKS_DIR: Path = Path(root_path) / "tests" / "benchmarks" / "components"

# Components whose to_code should run during benchmark builds.
# core/host/logger are infrastructure. json is needed because its
# to_code adds the ArduinoJson library (it's auto-loaded by api but
# cpp_testing suppresses to_code for components not in this set).
BENCHMARK_CODEGEN_COMPONENTS = {"core", "host", "logger", "json"}

PLATFORMIO_OPTIONS = {
    "build_flags": [
        "-DUSE_TIME_TIMEZONE",  # enable timezone code paths
        "-g",  # debug symbols for profiling
    ],
}


def run_benchmarks(selected_components: list[str], build_only: bool = False) -> int:
    return build_and_run(
        selected_components=selected_components,
        tests_dir=BENCHMARKS_DIR,
        codegen_components=BENCHMARK_CODEGEN_COMPONENTS,
        config_prefix="cppbench",
        friendly_name="CPP Benchmarks",
        libraries=PLATFORMIO_GOOGLE_BENCHMARK_LIB,
        platformio_options=PLATFORMIO_OPTIONS,
        main_entry="main.cpp",
        label="benchmarks",
        build_only=build_only,
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
