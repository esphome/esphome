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
from esphome.const import CONF_PLATFORM
from esphome.core import CORE
from esphome.loader import get_component
from esphome.platformio_api import get_idedata

# This must coincide with the version in /platformio.ini
PLATFORMIO_GOOGLE_TEST_LIB = "google/googletest@^1.15.2"

# Path to /tests/components
COMPONENTS_TESTS_DIR: Path = Path(root_path) / "tests" / "components"

# Components whose to_code should run during C++ test builds.
# Most components don't need code generation for tests; only these
# essential ones (platform setup, logging, core config) are needed.
# Note: "core" is the esphome core config module (esphome/core/config.py),
# which registers under package name "core" not "esphome".
CPP_TESTING_CODEGEN_COMPONENTS = {"core", "host", "logger"}


def hash_components(components: list[str]) -> str:
    key = ",".join(components)
    return hashlib.sha256(key.encode()).hexdigest()[:16]


def filter_components_without_tests(components: list[str]) -> list[str]:
    """Filter out components that do not have a corresponding test file.

    This is done by checking if the component's directory contains at
    least a .cpp or .h file.
    """
    filtered_components: list[str] = []
    for component in components:
        test_dir = COMPONENTS_TESTS_DIR / component
        if test_dir.is_dir() and (
            any(test_dir.glob("*.cpp")) or any(test_dir.glob("*.h"))
        ):
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
            },
            "includes": includes,
        },
        "host": {},
        "logger": {"level": "DEBUG"},
    }


def get_platform_components(components: list[str]) -> list[str]:
    """Discover platform sub-components referenced by test directory structure.

    For each component being tested, any sub-directory named after a platform
    domain (e.g. ``sensor``, ``binary_sensor``) is treated as a request to
    include that ``<domain>.<component>`` platform in the build.  The sub-
    directory must name a valid platform domain; anything else raises an error
    so that typos are caught early.

    Returns:
        List of ``"domain.component"`` strings, one per discovered sub-directory.
    """
    platform_components: list[str] = []
    for component in components:
        test_dir = COMPONENTS_TESTS_DIR / component
        if not test_dir.is_dir():
            continue
        # Each sub-directory name is expected to be a platform domain
        # (e.g. tests/components/bthome/sensor/ → sensor.bthome).
        for domain_dir in test_dir.iterdir():
            if not domain_dir.is_dir():
                continue
            domain = domain_dir.name
            domain_module = get_component(domain)
            if domain_module is None or not domain_module.is_platform_component:
                raise ValueError(
                    f"Component tests for '{component}' reference non-existing or invalid domain '{domain}'"
                    f" in its directory structure. See ({COMPONENTS_TESTS_DIR / component / domain})."
                )
            platform_components.append(f"{domain}.{component}")
    return platform_components


# Exit codes for run_tests
EXIT_OK = 0
EXIT_SKIPPED = 1
EXIT_COMPILE_ERROR = 2
EXIT_CONFIG_ERROR = 3
EXIT_NO_EXECUTABLE = 4


def run_tests(selected_components: list[str]) -> int:
    # Skip tests on Windows
    if os.name == "nt":
        print("Skipping esphome tests on Windows", file=sys.stderr)
        return EXIT_SKIPPED

    # Remove components that do not have tests
    components = filter_components_without_tests(selected_components)

    if len(components) == 0:
        print(
            "No components specified or no tests found for the specified components.",
            file=sys.stderr,
        )
        return EXIT_OK

    components = sorted(components)

    # Build a list of include folders relative to COMPONENTS_TESTS_DIR. These folders will
    # be added along with their subfolders.
    # "main.cpp" is a special entry that points to /tests/components/main.cpp,
    # which provides a custom test runner entry-point replacing the default one.
    # Each remaining entry is a component folder whose *.cpp files are compiled.
    includes: list[str] = ["main.cpp"] + components

    # Obtain a list of platform components to be tested:
    try:
        platform_components = get_platform_components(components)
    except ValueError as e:
        print(f"Error obtaining platform components: {e}")
        return EXIT_CONFIG_ERROR

    components = sorted(components + platform_components)

    # Create a unique name for this config based on the actual components being tested
    # to maximize cache during testing
    config_name: str = "cpptests-" + hash_components(components)

    # Obtain possible dependencies for the requested components.
    # Always include 'time' because USE_TIME_TIMEZONE is defined as a build flag,
    # which causes core/time.h to include components/time/posix_tz.h.
    components_with_dependencies: list[str] = sorted(
        get_all_dependencies(set(components) | {"time"}, cpp_testing=True)
    )

    config = create_test_config(config_name, includes)

    CORE.config_path = COMPONENTS_TESTS_DIR / "dummy.yaml"
    CORE.dashboard = None
    CORE.cpp_testing = True
    CORE.cpp_testing_codegen = CPP_TESTING_CODEGEN_COMPONENTS

    # Validate config will expand the above with defaults:
    config = validate_config(config, {})

    # Add all components and dependencies to the base configuration after validation, so their files
    # are added to the build.
    for component_name in components_with_dependencies:
        if "." in component_name:
            # Format is always "domain.component" (exactly one dot),
            # as produced by get_platform_components().
            domain, component = component_name.split(".", maxsplit=1)
            domain_list = config.setdefault(domain, [])
            CORE.testing_ensure_platform_registered(domain)
            domain_list.append({CONF_PLATFORM: component})
        else:
            config.setdefault(component_name, [])

    dependencies = set(components_with_dependencies) - set(components)
    deps_str = ", ".join(dependencies) if dependencies else "None"
    print(f"Testing components: {', '.join(components)}. Dependencies: {deps_str}")
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
        return EXIT_COMPILE_ERROR

    # After a successful compilation, locate the executable and run it:
    idedata = get_idedata(config)
    if idedata is None:
        print("Cannot find executable")
        return EXIT_NO_EXECUTABLE

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
