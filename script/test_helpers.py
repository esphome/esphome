"""Shared helpers for C++ unit test and benchmark build scripts."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import subprocess
import sys

from helpers import get_all_dependencies
import yaml

from esphome.__main__ import command_compile, parse_args
from esphome.config import validate_config
from esphome.const import CONF_PLATFORM
from esphome.core import CORE
from esphome.loader import get_component
from esphome.platformio_api import get_idedata

# This must coincide with the version in /platformio.ini
PLATFORMIO_GOOGLE_TEST_LIB = "google/googletest@^1.15.2"

# Google Benchmark library for PlatformIO
# Format: name=repository_url (see esphome/core/config.py library parsing)
PLATFORMIO_GOOGLE_BENCHMARK_LIB = (
    "benchmark=https://github.com/google/benchmark.git#v1.9.1"
)

# Key names for the base config sections
ESPHOME_KEY = "esphome"
HOST_KEY = "host"
LOGGER_KEY = "logger"

# Base config keys that are always present and must not be fully overridden
# by component benchmark.yaml files. esphome: allows sub-key merging.
BASE_CONFIG_KEYS = frozenset({ESPHOME_KEY, HOST_KEY, LOGGER_KEY})

# Shared build flag — enables timezone code paths for testing/benchmarking.
USE_TIME_TIMEZONE_FLAG = "-DUSE_TIME_TIMEZONE"

# Components whose to_code should always run during C++ test/benchmark builds.
# These are the minimal infrastructure components needed for host compilation.
# Note: "core" is the esphome core config module (esphome/core/config.py),
# which registers under package name "core" not "esphome".
BASE_CODEGEN_COMPONENTS = {"core", "host", "logger"}

# Exit codes
EXIT_OK = 0
EXIT_SKIPPED = 1
EXIT_COMPILE_ERROR = 2
EXIT_CONFIG_ERROR = 3
EXIT_NO_EXECUTABLE = 4

# Name of the per-component YAML config file in benchmark directories
BENCHMARK_YAML_FILENAME = "benchmark.yaml"


def hash_components(components: list[str]) -> str:
    """Create a short hash of component names for unique config naming."""
    key = ",".join(components)
    return hashlib.sha256(key.encode()).hexdigest()[:16]


def filter_components_with_files(components: list[str], tests_dir: Path) -> list[str]:
    """Filter out components that do not have .cpp or .h files in the tests dir.

    Args:
        components: List of component names to check
        tests_dir: Base directory containing component test/benchmark folders

    Returns:
        Filtered list of components that have test files
    """
    filtered_components: list[str] = []
    for component in components:
        test_dir = tests_dir / component
        if test_dir.is_dir() and (
            any(test_dir.glob("*.cpp")) or any(test_dir.glob("*.h"))
        ):
            filtered_components.append(component)
        else:
            print(
                f"WARNING: No files found for component '{component}' in {test_dir}, skipping.",
                file=sys.stderr,
            )
    return filtered_components


def get_platform_components(components: list[str], tests_dir: Path) -> list[str]:
    """Discover platform sub-components referenced by test directory structure.

    For each component, any sub-directory named after a platform domain
    (e.g. ``sensor``, ``binary_sensor``) is treated as a request to include
    that ``<domain>.<component>`` platform in the build.

    Args:
        components: List of component names to scan
        tests_dir: Base directory containing component test/benchmark folders

    Returns:
        List of ``"domain.component"`` strings
    """
    platform_components: list[str] = []
    for component in components:
        test_dir = tests_dir / component
        if not test_dir.is_dir():
            continue
        for domain_dir in test_dir.iterdir():
            if not domain_dir.is_dir():
                continue
            domain = domain_dir.name
            domain_module = get_component(domain)
            if domain_module is None or not domain_module.is_platform_component:
                raise ValueError(
                    f"Component '{component}' references non-existing or invalid domain '{domain}'"
                    f" in its directory structure. See ({tests_dir / component / domain})."
                )
            platform_components.append(f"{domain}.{component}")
    return platform_components


def load_component_yaml_configs(components: list[str], tests_dir: Path) -> dict:
    """Load and merge benchmark.yaml files from component directories.

    Each component directory may contain a ``benchmark.yaml`` file that
    declares additional ESPHome components needed for the build (e.g.
    ``api:``, ``sensor:``). These get merged into the base config before
    validation so that dependencies are properly resolved with defaults.

    The ``esphome:`` key is special: its sub-keys are merged into the
    existing esphome config (e.g. to add ``areas:`` or ``devices:``).
    Other base config keys (``host:``, ``logger:``) are not overridable.

    Args:
        components: List of component directory names
        tests_dir: Base directory containing component folders

    Returns:
        Merged dict of component configs to add to the base config
    """
    # Note: components are processed in sorted order. For conflicting keys
    # (e.g. two benchmark.yaml files both declaring sensor:), the first
    # component alphabetically wins via setdefault(). This is fine for now
    # with a single benchmark component (api) but would need a real merge
    # strategy if multiple components declare overlapping configs.
    merged: dict = {}
    for component in components:
        yaml_path = tests_dir / component / BENCHMARK_YAML_FILENAME
        if not yaml_path.is_file():
            continue
        with open(yaml_path) as f:
            component_config = yaml.safe_load(f)
        if component_config and isinstance(component_config, dict):
            for key, value in component_config.items():
                if key in BASE_CONFIG_KEYS - {ESPHOME_KEY}:
                    # host: and logger: are not overridable
                    continue
                if key == ESPHOME_KEY and isinstance(value, dict):
                    # Merge esphome sub-keys rather than replacing
                    esphome_extra = merged.setdefault(ESPHOME_KEY, {})
                    for sub_key, sub_value in value.items():
                        esphome_extra.setdefault(sub_key, sub_value)
                    continue
                merged.setdefault(key, value)
    return merged


def create_host_config(
    config_name: str,
    friendly_name: str,
    libraries: str | list[str],
    includes: list[str],
    platformio_options: dict,
) -> dict:
    """Create an ESPHome host configuration for C++ builds.

    Args:
        config_name: Unique name for this configuration
        friendly_name: Human-readable name
        libraries: PlatformIO library specification(s)
        includes: List of include folders for the build
        platformio_options: Dict of platformio_options to set

    Returns:
        Configuration dict for ESPHome
    """
    return {
        ESPHOME_KEY: {
            "name": config_name,
            "friendly_name": friendly_name,
            "libraries": libraries,
            "platformio_options": platformio_options,
            "includes": includes,
        },
        HOST_KEY: {},
        LOGGER_KEY: {"level": "DEBUG"},
    }


def compile_and_get_binary(
    config: dict,
    components: list[str],
    codegen_components: set[str],
    tests_dir: Path,
    label: str = "build",
) -> tuple[int, str | None]:
    """Compile an ESPHome configuration and return the binary path.

    Args:
        config: ESPHome configuration dict (already created via create_host_config)
        components: List of components to include in the build
        codegen_components: Set of component names whose to_code should run
        tests_dir: Base directory for test files (used as config_path base)
        label: Label for log messages (e.g. "unit tests", "benchmarks")

    Returns:
        Tuple of (exit_code, program_path_or_none)
    """
    # Load any benchmark.yaml files from component directories and merge
    # them into the config BEFORE dependency resolution and validation.
    # This allows each benchmark/test dir to declare which ESPHome components
    # it needs (e.g. api:) so they get proper config defaults.
    extra_config = load_component_yaml_configs(components, tests_dir)
    for key, value in extra_config.items():
        if key == ESPHOME_KEY and isinstance(value, dict):
            # Merge esphome sub-keys into existing esphome config.
            # For list values (e.g. libraries), extend rather than replace.
            for sub_key, sub_value in value.items():
                existing = config[ESPHOME_KEY].get(sub_key)
                if existing is not None and isinstance(sub_value, list):
                    # Ensure existing is a list, then extend
                    if not isinstance(existing, list):
                        config[ESPHOME_KEY][sub_key] = [existing]
                    config[ESPHOME_KEY][sub_key].extend(sub_value)
                else:
                    config[ESPHOME_KEY].setdefault(sub_key, sub_value)
        else:
            config.setdefault(key, value)

    # Obtain possible dependencies BEFORE validate_config, because
    # get_all_dependencies calls CORE.reset() which clears build_path.
    # Always include 'time' because USE_TIME_TIMEZONE is defined as a build flag,
    # which causes core/time.h to include components/time/posix_tz.h.
    components_with_dependencies: list[str] = sorted(
        get_all_dependencies(set(components) | {"time"}, cpp_testing=True)
    )

    CORE.config_path = tests_dir / "dummy.yaml"
    CORE.dashboard = None
    CORE.cpp_testing = True
    CORE.cpp_testing_codegen = codegen_components

    # Validate config will expand the above with defaults:
    config = validate_config(config, {})

    # Add remaining components and dependencies to the configuration after
    # validation, so their source files are included in the build.
    for component_name in components_with_dependencies:
        if "." in component_name:
            domain, component = component_name.split(".", maxsplit=1)
            domain_list = config.setdefault(domain, [])
            CORE.testing_ensure_platform_registered(domain)
            domain_list.append({CONF_PLATFORM: component})
        # Skip "core" — it's a pseudo-component handled by the build
        # system, not a real loadable component (get_component returns None)
        elif get_component(component_name) is not None:
            config.setdefault(component_name, [])

    # Register platforms from the extra config (benchmark.yaml) so
    # USE_SENSOR, USE_LIGHT, etc. defines are emitted without needing
    # real entity instances.
    for key in extra_config:
        if key == ESPHOME_KEY:
            continue
        comp = get_component(key)
        if comp is not None and comp.is_platform_component:
            CORE.testing_ensure_platform_registered(key)

    dependencies = set(components_with_dependencies) - set(components)
    deps_str = ", ".join(dependencies) if dependencies else "None"
    print(f"Building {label}: {', '.join(components)}. Dependencies: {deps_str}")
    CORE.config = config
    args = parse_args(["program", "compile", str(CORE.config_path)])
    try:
        exit_code: int = command_compile(args, config)

        if exit_code != 0:
            print(f"Error compiling {label} for {', '.join(components)}")
            return exit_code, None
    except Exception as e:
        print(f"Error compiling {label} for {', '.join(components)}: {e}")
        return EXIT_COMPILE_ERROR, None

    # After a successful compilation, locate the executable:
    idedata = get_idedata(config)
    if idedata is None:
        print("Cannot find executable")
        return EXIT_NO_EXECUTABLE, None

    program_path: str = idedata.raw["prog_path"]
    return EXIT_OK, program_path


def build_and_run(
    selected_components: list[str],
    tests_dir: Path,
    codegen_components: set[str],
    config_prefix: str,
    friendly_name: str,
    libraries: str | list[str],
    platformio_options: dict,
    main_entry: str,
    label: str = "build",
    build_only: bool = False,
    extra_run_args: list[str] | None = None,
    extra_include_dirs: list[Path] | None = None,
) -> int:
    """Build and optionally run a C++ test/benchmark binary.

    This is the main orchestration function shared between unit tests
    and benchmarks.

    Args:
        selected_components: Components to include (directory names in tests_dir)
        tests_dir: Directory containing test/benchmark files
        codegen_components: Components whose to_code should run
        config_prefix: Prefix for the config name (e.g. "cpptests", "cppbench")
        friendly_name: Human-readable name for the config
        libraries: PlatformIO library specification(s)
        platformio_options: PlatformIO options dict
        main_entry: Name of the main entry file (e.g. "main.cpp")
        label: Label for log messages
        build_only: If True, print binary path and return without running
        extra_run_args: Extra arguments to pass to the binary
        extra_include_dirs: Additional directories whose .cpp files
            should be compiled (resolved relative to tests_dir if possible)

    Returns:
        Exit code
    """
    # Skip on Windows
    if os.name == "nt":
        print(f"Skipping {label} on Windows", file=sys.stderr)
        return EXIT_SKIPPED

    # Remove components that do not have files
    components = filter_components_with_files(selected_components, tests_dir)

    if len(components) == 0:
        print(
            f"No components specified or no files found for {label}.",
            file=sys.stderr,
        )
        return EXIT_OK

    components = sorted(components)

    # Build include list: main entry point + component folders + extra dirs
    includes: list[str] = [main_entry] + components
    if extra_include_dirs:
        for d in extra_include_dirs:
            if d.is_dir() and (any(d.glob("*.cpp")) or any(d.glob("*.h"))):
                # ESPHome includes are relative to the config directory (tests_dir)
                rel = os.path.relpath(d, tests_dir)
                includes.append(rel)

    # Discover platform sub-components
    try:
        platform_components = get_platform_components(components, tests_dir)
    except ValueError as e:
        print(f"Error obtaining platform components: {e}")
        return EXIT_CONFIG_ERROR

    components = sorted(components + platform_components)

    # Create unique config name
    config_name: str = f"{config_prefix}-" + hash_components(components)

    config = create_host_config(
        config_name, friendly_name, libraries, includes, platformio_options
    )

    exit_code, program_path = compile_and_get_binary(
        config, components, codegen_components, tests_dir, label
    )

    if exit_code != EXIT_OK or program_path is None:
        return exit_code

    if build_only:
        print(f"BUILD_BINARY={program_path}")
        return EXIT_OK

    # Run the binary
    run_cmd: list[str] = [program_path]
    if extra_run_args:
        run_cmd.extend(extra_run_args)
    run_proc = subprocess.run(run_cmd, check=False)
    return run_proc.returncode
