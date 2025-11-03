#!/usr/bin/env python3
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys

from helpers import get_all_components, get_all_dependencies, root_path

from esphome.__main__ import command_compile, parse_args
from esphome.config import validate_config
from esphome.core import CORE
from esphome.platformio_api import get_idedata

# This must coincide with the version in /platformio.ini
PLATFORMIO_GOOGLE_TEST_LIB = "google/googletest@^1.15.2"

# Path to /tests/components
COMPONENTS_TESTS_DIR: Path = Path(root_path) / "tests" / "components"


def hash_components(components: list[str]) -> str:
    key = ",".join(components)
    return hashlib.sha256(key.encode()).hexdigest()[:16]


def filter_components_without_tests(components: list[str]) -> list[str]:
    """Filter out components that do not have a corresponding test file.

    This is done by checking if the component's directory contains at
    least a .cpp file.
    """
    filtered_components: list[str] = []
    for component in components:
        test_dir = COMPONENTS_TESTS_DIR / component
        if test_dir.is_dir() and any(test_dir.glob("*.cpp")):
            filtered_components.append(component)
        else:
            print(
                f"WARNING: No tests found for component '{component}', skipping.",
                file=sys.stderr,
            )
    return filtered_components


def create_test_config(config_name: str, includes: list[str]) -> dict:
    """Create ESPHome test configuration for C++ unit tests.

    Args:
        config_name: Unique name for this test configuration
        includes: List of include folders for the test build

    Returns:
        Configuration dict for ESPHome
    """
    return {
        "esphome": {
            "name": config_name,
            "friendly_name": "CPP Unit Tests",
            "libraries": PLATFORMIO_GOOGLE_TEST_LIB,
            "platformio_options": {
                "build_type": "debug",
                "build_unflags": [
                    "-Os",  # remove size-opt flag
                ],
                "build_flags": [
                    "-Og",  # optimize for debug
                ],
                "debug_build_flags": [  # only for debug builds
                    "-g3",  # max debug info
                    "-ggdb3",
                ],
            },
            "includes": includes,
        },
        "host": {},
        "logger": {"level": "DEBUG"},
    }


def run_tests(selected_components: list[str]) -> int:
    # Skip tests on Windows
    if os.name == "nt":
        print("Skipping esphome tests on Windows", file=sys.stderr)
        return 1

    # Remove components that do not have tests
    components = filter_components_without_tests(selected_components)

    if len(components) == 0:
        print(
            "No components specified or no tests found for the specified components.",
            file=sys.stderr,
        )
        return 0

    components = sorted(components)

    # Obtain possible dependencies for the requested components:
    components_with_dependencies = sorted(get_all_dependencies(set(components)))

    # Build a list of include folders, one folder per component containing tests.
    # A special replacement main.cpp is located in /tests/components/main.cpp
    includes: list[str] = ["main.cpp"] + components

    # Create a unique name for this config based on the actual components being tested
    # to maximize cache during testing
    config_name: str = "cpptests-" + hash_components(components)

    config = create_test_config(config_name, includes)

    CORE.config_path = COMPONENTS_TESTS_DIR / "dummy.yaml"
    CORE.dashboard = None

    # Validate config will expand the above with defaults:
    config = validate_config(config, {})

    # Add all components and dependencies to the base configuration after validation, so their files
    # are added to the build.
    config.update({key: {} for key in components_with_dependencies})

    print(f"Testing components: {', '.join(components)}")
    CORE.config = config
    args = parse_args(["program", "compile", str(CORE.config_path)])
    try:
        exit_code: int = command_compile(args, config)

        if exit_code != 0:
            print(f"Error compiling unit tests for {', '.join(components)}")
            return exit_code
    except Exception as e:
        print(
            f"Error compiling unit tests for {', '.join(components)}. Check path. : {e}"
        )
        return 2

    # After a successful compilation, locate the executable and run it:
    idedata = get_idedata(config)
    if idedata is None:
        print("Cannot find executable")
        return 1

    program_path: str = idedata.raw["prog_path"]
    run_cmd: list[str] = [program_path]
    run_proc = subprocess.run(run_cmd, check=False)
    return run_proc.returncode


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
