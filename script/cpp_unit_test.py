#!/usr/bin/env python3
import argparse
from functools import partial
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from build_helpers import (
    PLATFORMIO_GOOGLE_TEST_LIB,
    build_and_run,
    load_test_manifest_overrides,
)
from helpers import get_all_components, root_path

from esphome.host.toolchain import find_compilers

# Path to /tests/components
COMPONENTS_TESTS_DIR: Path = Path(root_path) / "tests" / "components"

BUILD_FLAGS = [
    "-Og",  # optimize for debug
    "-g3",  # max debug info
    "-ggdb3",
    "-DESPHOME_DEBUG",  # enable debug assertions
    "-fno-omit-frame-pointer",
]
SANITIZER_FLAGS = ["-fsanitize=address", "-fsanitize=undefined"]


def sanitizer_flags() -> list[str]:
    """The sanitizer flags to build with: all of them, except on macOS the ones whose runtime
    does not start. Apple clang 15's AddressSanitizer aborts before main() on macOS 26, which
    would fail every test for a toolchain problem, so each flag is tried on an empty program
    first. CI keeps every flag.
    """
    if sys.platform != "darwin":
        return SANITIZER_FLAGS
    cxx = find_compilers().cxx
    flags: list[str] = []
    with tempfile.TemporaryDirectory() as tmp:
        program = Path(tmp) / "probe"
        for flag in SANITIZER_FLAGS:
            result = subprocess.run(
                [*cxx, flag, "-x", "c++", "-", "-o", program],
                check=False,
                input=b"int main() { return 0; }",
                capture_output=True,
            )
            if result.returncode == 0:
                result = subprocess.run([program], check=False, capture_output=True)
            if result.returncode == 0:
                flags.append(flag)
                continue
            reason = result.stderr.decode(errors="replace").strip().splitlines()
            print(
                f"{flag} does not run with {cxx[0]}: "
                f"{reason[0] if reason else f'exit code {result.returncode}'}\n"
                "  building the tests without it; set CXX to a compiler whose runtime works to keep it",
                file=sys.stderr,
            )
    return flags


def run_tests(selected_components: list[str]) -> int:
    # allocator_may_return_null: an oversized request must come back empty, not abort the run
    os.environ["ASAN_OPTIONS"] = "detect_leaks=0:allocator_may_return_null=1"
    return build_and_run(
        selected_components=selected_components,
        tests_dir=COMPONENTS_TESTS_DIR,
        manifest_override_loader=partial(
            load_test_manifest_overrides, tests_dir=COMPONENTS_TESTS_DIR
        ),
        config_prefix="cpptests",
        friendly_name="CPP Unit Tests",
        libraries=PLATFORMIO_GOOGLE_TEST_LIB,
        build_flags=BUILD_FLAGS + sanitizer_flags(),
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
