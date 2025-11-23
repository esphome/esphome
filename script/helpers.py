from __future__ import annotations

from collections.abc import Callable
from functools import cache
import hashlib
import json
import os
import os.path
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any

import colorama

root_path = os.path.abspath(os.path.normpath(os.path.join(__file__, "..", "..")))
basepath = os.path.join(root_path, "esphome")
temp_folder = os.path.join(root_path, ".temp")
temp_header_file = os.path.join(temp_folder, "all-include.cpp")

# C++ file extensions used for clang-tidy and clang-format checks
CPP_FILE_EXTENSIONS = (".cpp", ".h", ".hpp", ".cc", ".cxx", ".c", ".tcc")

# Python file extensions
PYTHON_FILE_EXTENSIONS = (".py", ".pyi")

# Combined C++ and Python file extensions for convenience
CPP_AND_PYTHON_FILE_EXTENSIONS = (*CPP_FILE_EXTENSIONS, *PYTHON_FILE_EXTENSIONS)

# YAML file extensions
YAML_FILE_EXTENSIONS = (".yaml", ".yml")

# Component path prefix
ESPHOME_COMPONENTS_PATH = "esphome/components/"

# Test components path prefix
ESPHOME_TESTS_COMPONENTS_PATH = "tests/components/"

# Tuple of component and test paths for efficient startswith checks
COMPONENT_AND_TESTS_PATHS = (ESPHOME_COMPONENTS_PATH, ESPHOME_TESTS_COMPONENTS_PATH)

# Base bus components - these ARE the bus implementations and should not
# be flagged as needing migration since they are the platform/base components
BASE_BUS_COMPONENTS = {
    "i2c",
    "spi",
    "uart",
    "modbus",
    "canbus",
    "remote_transmitter",
    "remote_receiver",
}

# Cache version for components graph
# Increment this when the cache format or graph building logic changes
COMPONENTS_GRAPH_CACHE_VERSION = 1


def parse_list_components_output(output: str) -> list[str]:
    """Parse the output from list-components.py script.

    The script outputs one component name per line.

    Args:
        output: The stdout from list-components.py

    Returns:
        List of component names, or empty list if no output
    """
    if not output or not output.strip():
        return []
    return [c.strip() for c in output.strip().split("\n") if c.strip()]


def parse_test_filename(test_file: Path) -> tuple[str, str]:
    """Parse test filename to extract test name and platform.

    Test files follow the naming pattern: test.<platform>.yaml or test-<variant>.<platform>.yaml

    Args:
        test_file: Path to test file

    Returns:
        Tuple of (test_name, platform)
    """
    parts = test_file.stem.split(".")
    if len(parts) == 2:
        return parts[0], parts[1]  # test, platform
    return parts[0], "all"


def get_component_from_path(file_path: str) -> str | None:
    """Extract component name from a file path.

    Args:
        file_path: Path to a file (e.g., "esphome/components/wifi/wifi.cpp"
                                or "tests/components/uart/test.esp32-idf.yaml")

    Returns:
        Component name if path is in components or tests directory, None otherwise
    """
    if file_path.startswith(ESPHOME_COMPONENTS_PATH) or file_path.startswith(
        ESPHOME_TESTS_COMPONENTS_PATH
    ):
        parts = file_path.split("/")
        if len(parts) >= 3 and parts[2]:
            # Verify that parts[2] is actually a component directory, not a file
            # like .gitignore or README.md in the components directory itself
            component_name = parts[2]
            if "." not in component_name:
                return component_name
    return None


def get_component_test_files(
    component: str, *, all_variants: bool = False
) -> list[Path]:
    """Get test files for a component.

    Args:
        component: Component name (e.g., "wifi")
        all_variants: If True, returns all test files including variants (test-*.yaml).
                     If False, returns only base test files (test.*.yaml).
                     Default is False.

    Returns:
        List of test file paths for the component, or empty list if none exist
    """
    tests_dir = Path(root_path) / "tests" / "components" / component
    if not tests_dir.exists():
        return []

    if all_variants:
        # Match both test.*.yaml and test-*.yaml patterns
        return list(tests_dir.glob("test[.-]*.yaml"))
    # Match only test.*.yaml (base tests)
    return list(tests_dir.glob("test.*.yaml"))


def styled(color: str | tuple[str, ...], msg: str, reset: bool = True) -> str:
    prefix = "".join(color) if isinstance(color, tuple) else color
    suffix = colorama.Style.RESET_ALL if reset else ""
    return prefix + msg + suffix


def print_error_for_file(file: str | Path, body: str | None) -> None:
    print(
        styled(colorama.Fore.GREEN, "### File ")
        + styled((colorama.Fore.GREEN, colorama.Style.BRIGHT), str(file))
    )
    print()
    if body is not None:
        print(body)
        print()


def build_all_include() -> None:
    # Build a cpp file that includes all header files in this repo.
    # Otherwise header-only integrations would not be tested by clang-tidy

    # Use git ls-files to find all .h files in the esphome directory
    # This is much faster than walking the filesystem
    cmd = ["git", "ls-files", "esphome/**/*.h"]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)

    # Process git output - git already returns paths relative to repo root
    headers = [
        f'#include "{include_p}"'
        for line in proc.stdout.strip().split("\n")
        if (include_p := line.replace(os.path.sep, "/"))
    ]

    headers.sort()
    headers.append("")
    content = "\n".join(headers)
    p = Path(temp_header_file)
    p.parent.mkdir(exist_ok=True)
    p.write_text(content, encoding="utf-8")


def get_output(*args: str) -> str:
    with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as proc:
        output, _ = proc.communicate()
    return output.decode("utf-8")


def get_err(*args: str) -> str:
    with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as proc:
        _, err = proc.communicate()
    return err.decode("utf-8")


def splitlines_no_ends(string: str) -> list[str]:
    return [s.strip() for s in string.splitlines()]


@cache
def _get_github_event_data() -> dict | None:
    """Read and parse GitHub event file (cached).

    Returns:
        Parsed event data dictionary, or None if not available
    """
    github_event_path = os.environ.get("GITHUB_EVENT_PATH")
    if github_event_path and os.path.exists(github_event_path):
        with open(github_event_path) as f:
            return json.load(f)
    return None


def _get_pr_number_from_github_env() -> str | None:
    """Extract PR number from GitHub environment variables.

    Returns:
        PR number as string, or None if not found
    """
    # First try parsing GITHUB_REF (fastest)
    github_ref = os.environ.get("GITHUB_REF", "")
    if "/pull/" in github_ref:
        return github_ref.split("/pull/")[1].split("/")[0]

    # Fallback to GitHub event file
    if event_data := _get_github_event_data():
        pr_data = event_data.get("pull_request", {})
        if pr_number := pr_data.get("number"):
            return str(pr_number)

    return None


def get_target_branch() -> str | None:
    """Get the target branch from GitHub environment variables.

    Returns:
        Target branch name (e.g., "dev", "release", "beta"), or None if not in PR context
    """
    # First try GITHUB_BASE_REF (set for pull_request events)
    if base_ref := os.environ.get("GITHUB_BASE_REF"):
        return base_ref

    # Fallback to GitHub event file
    if event_data := _get_github_event_data():
        pr_data = event_data.get("pull_request", {})
        base_data = pr_data.get("base", {})
        if ref := base_data.get("ref"):
            return ref

    return None


@cache
def _get_changed_files_github_actions() -> list[str] | None:
    """Get changed files in GitHub Actions environment.

    Returns:
        List of changed files, or None if should fall back to git method
    """
    event_name = os.environ.get("GITHUB_EVENT_NAME")

    # For pull requests
    if event_name == "pull_request":
        pr_number = _get_pr_number_from_github_env()
        if pr_number:
            # Try gh pr diff first (faster for small PRs)
            cmd = ["gh", "pr", "diff", pr_number, "--name-only"]
            try:
                return _get_changed_files_from_command(cmd)
            except Exception as e:
                # If it fails due to the 300 file limit, use the API method
                if "maximum" in str(e) and "files" in str(e):
                    cmd = [
                        "gh",
                        "api",
                        f"repos/esphome/esphome/pulls/{pr_number}/files",
                        "--paginate",
                        "--jq",
                        ".[].filename",
                    ]
                    return _get_changed_files_from_command(cmd)
                # Re-raise for other errors
                raise

    # For pushes (including squash-and-merge)
    elif event_name == "push":
        # For push events, we want to check what changed in this commit
        try:
            # Get the changed files in the last commit
            return _get_changed_files_from_command(
                ["git", "diff", "HEAD~1..HEAD", "--name-only"]
            )
        except:  # noqa: E722
            # Fall back to the original method if this fails
            pass

    return None


def changed_files(branch: str | None = None) -> list[str]:
    # In GitHub Actions, we can use the API to get changed files more efficiently
    if os.environ.get("GITHUB_ACTIONS") == "true":
        github_files = _get_changed_files_github_actions()
        if github_files is not None:
            return github_files

    # Original implementation for local development
    if not branch:  # Treat None and empty string the same
        branch = "dev"
    check_remotes = ["upstream", "origin"]
    check_remotes.extend(splitlines_no_ends(get_output("git", "remote")))
    for remote in check_remotes:
        command = ["git", "merge-base", f"refs/remotes/{remote}/{branch}", "HEAD"]
        try:
            merge_base = splitlines_no_ends(get_output(*command))[0]
            break
        # pylint: disable=bare-except
        except:  # noqa: E722
            pass
    else:
        raise ValueError("Git not configured")
    return _get_changed_files_from_command(["git", "diff", merge_base, "--name-only"])


def _get_changed_files_from_command(command: list[str]) -> list[str]:
    """Run a git command to get changed files and return them as a list."""
    proc = subprocess.run(command, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        raise Exception(f"Command failed: {' '.join(command)}\nstderr: {proc.stderr}")

    changed_files = splitlines_no_ends(proc.stdout)
    changed_files = [os.path.relpath(f, os.getcwd()) for f in changed_files if f]
    changed_files.sort()
    return changed_files


def get_changed_components() -> list[str] | None:
    """Get list of changed components using list-components.py script.

    This function:
    1. First checks if any core C++/header files (esphome/core/*.{cpp,h,hpp,cc,cxx,c}) changed - if so, returns None
    2. Otherwise delegates to ./script/list-components.py --changed which:
       - Analyzes all changed files
       - Determines which components are affected (including dependencies)
       - Returns a list of component names that need to be checked

    Returns:
        - None: Core C++/header files changed, need full scan
        - Empty list: No components changed (only non-component files changed)
        - List of strings: Names of components that need checking (e.g., ["wifi", "mqtt"])
    """
    # Check if any core C++ or header files changed first
    changed = changed_files()
    core_cpp_changed = any(
        f.startswith("esphome/core/")
        and f.endswith(CPP_FILE_EXTENSIONS[:-1])  # Exclude .tcc for core files
        for f in changed
    )
    if core_cpp_changed:
        print(
            "Core C++/header files changed - will run full clang-tidy scan",
            file=sys.stderr,
        )
        return None

    # Use list-components.py to get changed components
    script_path = os.path.join(root_path, "script", "list-components.py")
    cmd = [script_path, "--changed"]

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=True, close_fds=False
        )
        return parse_list_components_output(result.stdout)
    except subprocess.CalledProcessError:
        # If the script fails, fall back to full scan
        print(
            "Could not determine changed components - will run full clang-tidy scan",
            file=sys.stderr,
        )
        return None


def _filter_changed_ci(files: list[str]) -> list[str]:
    """Filter files based on changed components in CI environment.

    This function implements intelligent filtering to reduce CI runtime by only
    checking files that could be affected by the changes. It handles three scenarios:

    1. Core C++/header files changed (returns None from get_changed_components):
       - Triggered when any C++/header file in esphome/core/ is modified
       - Action: Check ALL files (full scan)
       - Reason: Core C++/header files are used throughout the codebase

    2. No components changed (returns empty list from get_changed_components):
       - Triggered when only non-component files changed (e.g., scripts, configs)
       - Action: Check only the specific non-component files that changed
       - Example: If only script/clang-tidy changed, only check that file

    3. Specific components changed (returns list of component names):
       - Component detection done by: ./script/list-components.py --changed
       - That script analyzes which components are affected by the changed files
         INCLUDING their dependencies
       - Action: Check ALL files in each component that list-components.py identifies
       - Example: If wifi.cpp changed, list-components.py might return ["wifi", "network"]
                 if network depends on wifi. We then check ALL files in both
                 esphome/components/wifi/ and esphome/components/network/
       - Reason: Component files often have interdependencies (headers, base classes)

    Args:
        files: List of all files that clang-tidy would normally check

    Returns:
        Filtered list of files to check
    """
    components = get_changed_components()
    if components is None:
        # Scenario 1: Core files changed or couldn't determine components
        # Action: Return all files for full scan
        return files

    if not components:
        # Scenario 2: No components changed - only non-component files changed
        # Action: Check only the specific non-component files that changed
        changed = changed_files()
        files = [
            f
            for f in files
            if f in changed and not f.startswith(ESPHOME_COMPONENTS_PATH)
        ]
        if not files:
            print("No files changed", file=sys.stderr)
        return files

    # Scenario 3: Specific components changed
    # Action: Check ALL files in each changed component
    # Convert component list to set for O(1) lookups
    component_set = set(components)
    print(f"Changed components: {', '.join(sorted(components))}", file=sys.stderr)

    # The 'files' parameter contains ALL files in the codebase that clang-tidy would check.
    # We filter this down to only files in the changed components.
    # We check ALL files in each changed component (not just the changed files)
    # because changes in one file can affect other files in the same component.
    filtered_files = []
    for f in files:
        component = get_component_from_path(f)
        if component and component in component_set:
            filtered_files.append(f)

    return filtered_files


def _filter_changed_local(files: list[str]) -> list[str]:
    """Filter files based on git changes for local development.

    Args:
        files: List of all files to filter

    Returns:
        Filtered list of files to check
    """
    # For local development, just check changed files directly
    changed = changed_files()
    return [f for f in files if f in changed]


def filter_changed(files: list[str]) -> list[str]:
    """Filter files to only those that changed or are in changed components.

    Args:
        files: List of files to filter
    """
    # When running from CI, use component-based filtering
    if os.environ.get("GITHUB_ACTIONS") == "true":
        files = _filter_changed_ci(files)
    else:
        files = _filter_changed_local(files)

    print_file_list(files, "Files to check after filtering:")
    return files


def filter_grep(files: list[str], value: list[str]) -> list[str]:
    matched = []
    for file in files:
        with open(file, encoding="utf-8") as handle:
            contents = handle.read()
        if any(v in contents for v in value):
            matched.append(file)
    return matched


def git_ls_files(patterns: list[str] | None = None) -> dict[str, int]:
    command = ["git", "ls-files", "-s"]
    if patterns is not None:
        command.extend(patterns)
    with subprocess.Popen(command, stdout=subprocess.PIPE) as proc:
        output, _ = proc.communicate()
    lines = [x.split() for x in output.decode("utf-8").splitlines()]
    return {s[3].strip(): int(s[0]) for s in lines}


def load_idedata(environment: str) -> dict[str, Any]:
    start_time = time.time()
    print(f"Loading IDE data for environment '{environment}'...")

    platformio_ini = Path(root_path) / "platformio.ini"
    temp_idedata = Path(temp_folder) / f"idedata-{environment}.json"
    changed = False
    if (
        not platformio_ini.is_file()
        or not temp_idedata.is_file()
        or platformio_ini.stat().st_mtime >= temp_idedata.stat().st_mtime
    ):
        changed = True

    if "idf" in environment:
        # remove full sdkconfig when the defaults have changed so that it is regenerated
        default_sdkconfig = Path(root_path) / "sdkconfig.defaults"
        temp_sdkconfig = Path(temp_folder) / f"sdkconfig-{environment}"

        if not temp_sdkconfig.is_file():
            changed = True
        elif default_sdkconfig.stat().st_mtime >= temp_sdkconfig.stat().st_mtime:
            temp_sdkconfig.unlink()
            changed = True

    if not changed:
        data = json.loads(temp_idedata.read_text())
        elapsed = time.time() - start_time
        print(f"IDE data loaded from cache in {elapsed:.2f} seconds")
        return data

    # ensure temp directory exists before running pio, as it writes sdkconfig to it
    Path(temp_folder).mkdir(exist_ok=True)

    if "nrf" in environment:
        from helpers_zephyr import load_idedata as zephyr_load_idedata

        data = zephyr_load_idedata(environment, temp_folder, platformio_ini)
    else:
        stdout = subprocess.check_output(
            ["pio", "run", "-t", "idedata", "-e", environment]
        )
        match = re.search(r'{\s*".*}', stdout.decode("utf-8"))
        data = json.loads(match.group())
    temp_idedata.write_text(json.dumps(data, indent=2) + "\n")

    elapsed = time.time() - start_time
    print(f"IDE data generated and cached in {elapsed:.2f} seconds")
    return data


def get_binary(name: str, version: str) -> str:
    binary_file = f"{name}-{version}"
    try:
        result = subprocess.check_output([binary_file, "-version"])
        return binary_file
    except FileNotFoundError:
        pass
    binary_file = name
    try:
        result = subprocess.run(
            [binary_file, "-version"], text=True, capture_output=True, check=False
        )
        if result.returncode == 0 and (f"version {version}") in result.stdout:
            return binary_file
        raise FileNotFoundError(f"{name} not found")

    except FileNotFoundError:
        print(
            f"""
            Oops. It looks like {name} is not installed. It should be available under venv/bin
            and in PATH after running in turn:
              script/setup
              source venv/bin/activate.

            Please confirm you can run "{name} -version" or "{name}-{version} -version"
            in your terminal and install
            {name} (v{version}) if necessary.

            Note you can also upload your code as a pull request on GitHub and see the CI check
            output to apply {name}
            """
        )
        raise


def print_file_list(
    files: list[str], title: str = "Files:", max_files: int = 20
) -> None:
    """Print a list of files with optional truncation for large lists.

    Args:
        files: List of file paths to print
        title: Title to print before the list
        max_files: Maximum number of files to show before truncating (default: 20)
    """
    print(title)
    if not files:
        print("    No files to check!")
    elif len(files) <= max_files:
        for f in sorted(files):
            print(f"    {f}")
    else:
        sorted_files = sorted(files)
        for f in sorted_files[:10]:
            print(f"    {f}")
        print(f"    ... and {len(files) - 10} more files")


def get_usable_cpu_count() -> int:
    """Return the number of CPUs that can be used for processes.

    On Python 3.13+ this is the number of CPUs that can be used for processes.
    On older Python versions this is the number of CPUs.
    """
    return (
        os.process_cpu_count() if hasattr(os, "process_cpu_count") else os.cpu_count()
    )


def get_all_dependencies(component_names: set[str]) -> set[str]:
    """Get all dependencies for a set of components.

    Args:
        component_names: Set of component names to get dependencies for

    Returns:
        Set of all components including dependencies and auto-loaded components
    """
    from esphome.const import KEY_CORE
    from esphome.core import CORE
    from esphome.loader import get_component

    all_components: set[str] = set(component_names)

    # Reset CORE to ensure clean state
    CORE.reset()

    # Set up fake config path for component loading
    root = Path(__file__).parent.parent
    CORE.config_path = root
    CORE.data[KEY_CORE] = {}

    # Keep finding dependencies until no new ones are found
    while True:
        new_components: set[str] = set()

        for comp_name in all_components:
            comp = get_component(comp_name)
            if not comp:
                continue

            # Add dependencies (extract component name before '.')
            new_components.update(dep.split(".")[0] for dep in comp.dependencies)

            # Add auto_load components
            auto_load = comp.auto_load
            if callable(auto_load):
                import inspect

                if inspect.signature(auto_load).parameters:
                    auto_load = auto_load(None)
                else:
                    auto_load = auto_load()

            new_components.update(auto_load)

        # Check if we found any new components
        new_components -= all_components
        if not new_components:
            break

        all_components.update(new_components)

    return all_components


def get_components_from_integration_fixtures() -> set[str]:
    """Extract all components used in integration test fixtures.

    Returns:
        Set of component names used in integration test fixtures
    """
    from esphome import yaml_util

    components: set[str] = set()
    fixtures_dir = Path(__file__).parent.parent / "tests" / "integration" / "fixtures"

    for yaml_file in fixtures_dir.glob("*.yaml"):
        config: dict[str, any] | None = yaml_util.load_yaml(yaml_file)
        if not config:
            continue

        # Add all top-level component keys
        components.update(config.keys())

        # Add platform components (e.g., output.template)
        for value in config.values():
            if not isinstance(value, list):
                continue

            for item in value:
                if isinstance(item, dict) and "platform" in item:
                    components.add(item["platform"])

    return components


def filter_component_and_test_files(file_path: str) -> bool:
    """Check if a file path is a component or test file.

    Args:
        file_path: Path to check

    Returns:
        True if the file is in a component or test directory
    """
    return file_path.startswith(COMPONENT_AND_TESTS_PATHS) or (
        file_path.startswith(ESPHOME_TESTS_COMPONENTS_PATH)
        and file_path.endswith(YAML_FILE_EXTENSIONS)
    )


def filter_component_and_test_cpp_files(file_path: str) -> bool:
    """Check if a file is a C++ source file in component or test directories.

    Args:
        file_path: Path to check

    Returns:
        True if the file is a C++ source/header file in component or test directories
    """
    return file_path.endswith(CPP_FILE_EXTENSIONS) and file_path.startswith(
        COMPONENT_AND_TESTS_PATHS
    )


def extract_component_names_from_files(files: list[str]) -> list[str]:
    """Extract unique component names from a list of file paths.

    Args:
        files: List of file paths

    Returns:
        List of unique component names (preserves order)
    """
    return list(
        dict.fromkeys(comp for file in files if (comp := get_component_from_path(file)))
    )


def add_item_to_components_graph(
    components_graph: dict[str, list[str]], parent: str, child: str
) -> None:
    """Add a dependency relationship to the components graph.

    Args:
        components_graph: Graph mapping parent components to their children
        parent: Parent component name
        child: Child component name (dependent)
    """
    if not parent.startswith("__") and parent != child:
        if parent not in components_graph:
            components_graph[parent] = []
        if child not in components_graph[parent]:
            components_graph[parent].append(child)


def resolve_auto_load(
    auto_load: list[str] | Callable[[], list[str]] | Callable[[dict | None], list[str]],
    config: dict | None = None,
) -> list[str]:
    """Resolve AUTO_LOAD to a list, handling callables with or without config parameter.

    Args:
        auto_load: The AUTO_LOAD value (list or callable)
        config: Optional config to pass to callable AUTO_LOAD functions

    Returns:
        List of component names to auto-load
    """
    if not callable(auto_load):
        return auto_load

    import inspect

    if inspect.signature(auto_load).parameters:
        return auto_load(config)
    return auto_load()


@cache
def get_components_graph_cache_key() -> str:
    """Generate cache key based on all component Python file hashes.

    Uses git ls-files with sha1 hashes to generate a stable cache key that works
    across different machines and CI runs. This is faster and more reliable than
    reading file contents or using modification times.

    Returns:
        SHA256 hex string uniquely identifying the current component state
    """

    # Use git ls-files -s to get sha1 hashes of all component Python files
    # Format: <mode> <sha1> <stage> <path>
    # This is fast and works consistently across CI and local dev
    # We hash all .py files because AUTO_LOAD, DEPENDENCIES, etc. can be defined
    # in any Python file, not just __init__.py
    cmd = ["git", "ls-files", "-s", "esphome/components/**/*.py"]
    result = subprocess.run(
        cmd, capture_output=True, text=True, check=True, cwd=root_path, close_fds=False
    )

    # Hash the git output (includes file paths and their sha1 hashes)
    # This changes only when component Python files actually change
    hasher = hashlib.sha256()
    hasher.update(result.stdout.encode())

    return hasher.hexdigest()


def create_components_graph() -> dict[str, list[str]]:
    """Create a graph of component dependencies (cached).

    This function is expensive (5-6 seconds) because it imports all ESPHome components
    to extract their DEPENDENCIES and AUTO_LOAD metadata. The result is cached based
    on component file modification times, so unchanged components don't trigger a rebuild.

    Returns:
        Dictionary mapping parent components to their children (dependencies)
    """
    # Check cache first - use fixed filename since GitHub Actions cache doesn't support wildcards
    cache_file = Path(temp_folder) / "components_graph.json"

    if cache_file.exists():
        try:
            cached_data = json.loads(cache_file.read_text())
        except (OSError, json.JSONDecodeError):
            # Cache file corrupted or unreadable, rebuild
            pass
        else:
            # Verify cache version matches
            if cached_data.get("_version") == COMPONENTS_GRAPH_CACHE_VERSION:
                # Verify cache is for current component state
                cache_key = get_components_graph_cache_key()
                if cached_data.get("_cache_key") == cache_key:
                    return cached_data.get("graph", {})
                # Cache key mismatch - stale cache, rebuild
            # Cache version mismatch - incompatible format, rebuild

    from esphome import const
    from esphome.core import CORE
    from esphome.loader import ComponentManifest, get_component, get_platform

    # The root directory of the repo
    root = Path(root_path)
    components_dir = root / ESPHOME_COMPONENTS_PATH
    # Fake some directory so that get_component works
    CORE.config_path = root
    # Various configuration to capture different outcomes used by `AUTO_LOAD` function.
    KEY_CORE = const.KEY_CORE
    KEY_TARGET_FRAMEWORK = const.KEY_TARGET_FRAMEWORK
    KEY_TARGET_PLATFORM = const.KEY_TARGET_PLATFORM
    PLATFORM_ESP32 = const.PLATFORM_ESP32
    PLATFORM_ESP8266 = const.PLATFORM_ESP8266

    TARGET_CONFIGURATIONS = [
        {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: None},
        {KEY_TARGET_FRAMEWORK: "arduino", KEY_TARGET_PLATFORM: None},
        {KEY_TARGET_FRAMEWORK: "esp-idf", KEY_TARGET_PLATFORM: None},
        {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: PLATFORM_ESP32},
        {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: PLATFORM_ESP8266},
    ]
    CORE.data[KEY_CORE] = TARGET_CONFIGURATIONS[0]

    components_graph = {}
    platforms = []
    components: list[tuple[ComponentManifest, str, Path]] = []

    for path in components_dir.iterdir():
        if not path.is_dir():
            continue
        if not (path / "__init__.py").is_file():
            continue
        name = path.name
        comp = get_component(name)
        if comp is None:
            raise RuntimeError(
                f"Cannot find component {name}. Make sure current path is pip installed ESPHome"
            )

        components.append((comp, name, path))
        if comp.is_platform_component:
            platforms.append(name)

    platforms = set(platforms)

    for comp, name, path in components:
        for dependency in comp.dependencies:
            add_item_to_components_graph(
                components_graph, dependency.split(".")[0], name
            )

        for target_config in TARGET_CONFIGURATIONS:
            CORE.data[KEY_CORE] = target_config
            for item in resolve_auto_load(comp.auto_load, config=None):
                add_item_to_components_graph(components_graph, item, name)
        # restore config
        CORE.data[KEY_CORE] = TARGET_CONFIGURATIONS[0]

        for platform_path in path.iterdir():
            platform_name = platform_path.stem
            if platform_name == name or platform_name not in platforms:
                continue
            platform = get_platform(platform_name, name)
            if platform is None:
                continue

            add_item_to_components_graph(components_graph, platform_name, name)

            for dependency in platform.dependencies:
                add_item_to_components_graph(
                    components_graph, dependency.split(".")[0], name
                )

            for target_config in TARGET_CONFIGURATIONS:
                CORE.data[KEY_CORE] = target_config
                for item in resolve_auto_load(platform.auto_load, config={}):
                    add_item_to_components_graph(components_graph, item, name)
            # restore config
            CORE.data[KEY_CORE] = TARGET_CONFIGURATIONS[0]

    # Save to cache with version and cache key for validation
    cache_data = {
        "_version": COMPONENTS_GRAPH_CACHE_VERSION,
        "_cache_key": get_components_graph_cache_key(),
        "graph": components_graph,
    }
    cache_file.parent.mkdir(exist_ok=True)
    cache_file.write_text(json.dumps(cache_data))

    return components_graph


def find_children_of_component(
    components_graph: dict[str, list[str]], component_name: str, depth: int = 0
) -> list[str]:
    """Find all components that depend on the given component (recursively).

    Args:
        components_graph: Graph mapping parent components to their children
        component_name: Component name to find children for
        depth: Current recursion depth (max 10)

    Returns:
        List of all dependent component names (may contain duplicates removed at end)
    """
    if component_name not in components_graph:
        return []

    children = []

    for child in components_graph[component_name]:
        children.append(child)
        if depth < 10:
            children.extend(
                find_children_of_component(components_graph, child, depth + 1)
            )
    # Remove duplicate values
    return list(set(children))


def get_components_with_dependencies(
    files: list[str], get_dependencies: bool = False
) -> list[str]:
    """Get component names from files, optionally including their dependencies.

    Args:
        files: List of file paths
        get_dependencies: If True, include all dependent components

    Returns:
        Sorted list of component names
    """
    components = extract_component_names_from_files(files)

    if get_dependencies:
        components_graph = create_components_graph()

        all_components = components.copy()
        for c in components:
            all_components.extend(find_children_of_component(components_graph, c))
        # Remove duplicate values
        all_changed_components = list(set(all_components))

        return sorted(all_changed_components)

    return sorted(components)


def get_all_component_files() -> list[str]:
    """Get all component and test files from git.

    Returns:
        List of all component and test file paths
    """
    files = git_ls_files()
    return list(filter(filter_component_and_test_files, files))


def get_all_components() -> list[str]:
    """Get all component names.

    This function uses git to find all component files and extracts the component names.
    It returns the same list as calling list-components.py without arguments.

    Returns:
        List of all component names
    """
    return get_components_with_dependencies(get_all_component_files(), False)


def core_changed(files: list[str]) -> bool:
    """Check if any core C++ or Python files have changed.

    Args:
        files: List of file paths to check

    Returns:
        True if any core C++ or Python files have changed
    """
    return any(
        f.startswith("esphome/core/") and f.endswith(CPP_AND_PYTHON_FILE_EXTENSIONS)
        for f in files
    )


def get_cpp_changed_components(files: list[str]) -> list[str]:
    """Get components that have changed C++ files or tests.

    This function analyzes a list of changed files and determines which components
    are affected. It handles two scenarios:

    1. Test files changed (tests/components/<component>/*.cpp):
       - Adds the component to the affected list
       - Only that component needs to be tested

    2. Component C++ files changed (esphome/components/<component>/*):
       - Adds the component to the affected list
       - Also adds all components that depend on this component (recursively)
       - This ensures that changes propagate to dependent components

    Args:
        files: List of file paths to analyze (should be C++ files)

    Returns:
        Sorted list of component names that need C++ unit tests run
    """
    components_graph = create_components_graph()
    affected: set[str] = set()
    for file in files:
        if not file.endswith(CPP_FILE_EXTENSIONS):
            continue
        if file.startswith(ESPHOME_TESTS_COMPONENTS_PATH):
            parts = file.split("/")
            if len(parts) >= 4:
                component_dir = Path(ESPHOME_TESTS_COMPONENTS_PATH) / parts[2]
                if component_dir.is_dir():
                    affected.add(parts[2])
        elif file.startswith(ESPHOME_COMPONENTS_PATH):
            parts = file.split("/")
            if len(parts) >= 4:
                component = parts[2]
                affected.update(find_children_of_component(components_graph, component))
                affected.add(component)
    return sorted(affected)
