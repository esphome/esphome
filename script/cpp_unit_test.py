#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys

from helpers import get_all_components, root_path
from test_helpers import PLATFORMIO_GOOGLE_TEST_LIB, build_and_run

# Path to /tests/components
COMPONENTS_TESTS_DIR: Path = Path(root_path) / "tests" / "components"

# Components whose to_code should run during C++ test builds.
# Most components don't need code generation for tests; only these
# essential ones (platform setup, logging, core config) are needed.
# Note: "core" is the esphome core config module (esphome/core/config.py),
# which registers under package name "core" not "esphome".
CPP_TESTING_CODEGEN_COMPONENTS = {"core", "host", "logger"}

PLATFORMIO_OPTIONS = {
    "build_type": "debug",
    "build_unflags": [
        "-Os",  # remove size-opt flag
    ],
    "build_flags": [
        "-Og",  # optimize for debug
        "-DUSE_TIME_TIMEZONE",  # enable timezone code paths for testing
        "-DESPHOME_DEBUG",  # enable debug assertions
        # Enable the address and undefined behavior sanitizers
        "-fsanitize=address",
        "-fsanitize=undefined",
        "-fno-omit-frame-pointer",
    ],
    "debug_build_flags": [  # only for debug builds
        "-g3",  # max debug info
        "-ggdb3",
    ],
}


def run_tests(selected_components: list[str]) -> int:
    return build_and_run(
        selected_components=selected_components,
        tests_dir=COMPONENTS_TESTS_DIR,
        codegen_components=CPP_TESTING_CODEGEN_COMPONENTS,
        config_prefix="cpptests",
        friendly_name="CPP Unit Tests",
        libraries=PLATFORMIO_GOOGLE_TEST_LIB,
        platformio_options=PLATFORMIO_OPTIONS,
        main_entry="main.cpp",
        label="unit tests",
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run C++ unit tests for ESPHome components."
    )
    parser.add_argument(
        "components",
        nargs="*",
        help="List of components to test. Use --all to test all known components.",
    )
    parser.add_argument("--all", action="store_true", help="Test all known components.")

    args = parser.parse_args()

    if args.all:
        components: list[str] = get_all_components()
    else:
        components: list[str] = args.components

    sys.exit(run_tests(components))


if __name__ == "__main__":
    main()
