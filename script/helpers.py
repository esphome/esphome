from __future__ import annotations

from functools import cache
import json
import os
import os.path
from pathlib import Path
import re
import subprocess
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

# YAML file extensions
YAML_FILE_EXTENSIONS = (".yaml", ".yml")

# Component path prefix
ESPHOME_COMPONENTS_PATH = "esphome/components/"


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


def styled(color: str | tuple[str, ...], msg: str, reset: bool = True) -> str:
    prefix = "".join(color) if isinstance(color, tuple) else color
    suffix = colorama.Style.RESET_ALL if reset else ""
    return prefix + msg + suffix


def print_error_for_file(file: str, body: str | None) -> None:
    print(
        styled(colorama.Fore.GREEN, "### File ")
        + styled((colorama.Fore.GREEN, colorama.Style.BRIGHT), file)
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
    github_event_path = os.environ.get("GITHUB_EVENT_PATH")
    if github_event_path and os.path.exists(github_event_path):
        with open(github_event_path) as f:
            event_data = json.load(f)
            pr_data = event_data.get("pull_request", {})
            if pr_number := pr_data.get("number"):
                return str(pr_number)

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
        print("Core C++/header files changed - will run full clang-tidy scan")
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
        print("Could not determine changed components - will run full clang-tidy scan")
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
            print("No files changed")
        return files

    # Scenario 3: Specific components changed
    # Action: Check ALL files in each changed component
    # Convert component list to set for O(1) lookups
    component_set = set(components)
    print(f"Changed components: {', '.join(sorted(components))}")

    # The 'files' parameter contains ALL files in the codebase that clang-tidy would check.
    # We filter this down to only files in the changed components.
    # We check ALL files in each changed component (not just the changed files)
    # because changes in one file can affect other files in the same component.
    filtered_files = []
    for f in files:
        if f.startswith(ESPHOME_COMPONENTS_PATH):
            # Check if file belongs to any of the changed components
            parts = f.split("/")
            if len(parts) >= 3 and parts[2] in component_set:
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
    CORE.config_path = str(root)
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
            new_components.update(comp.auto_load)

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
        config: dict[str, any] | None = yaml_util.load_yaml(str(yaml_file))
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
