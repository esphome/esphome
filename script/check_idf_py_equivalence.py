#!/usr/bin/env python3
"""Check that a native ESP-IDF build tree is what idf.py itself would produce.

ESPHome runs cmake and ninja directly with the arguments idf.py uses. This
runs the real ``idf.py reconfigure`` and ``idf.py build`` on a finished tree
(in place: CMake rejects a moved cache) and fails if either one changes the
cache, the generated build files or the firmware, or recompiles anything.
It catches drift when the pinned ESP-IDF version changes what idf.py does.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

# Files that change if idf.py configures or builds differently.
WATCHED = (
    "build/CMakeCache.txt",
    "build/build.ninja",
    "build/compile_commands.json",
    "build/project_description.json",
    "build/config/sdkconfig.h",
    "build/esp_idf_size.json",
)
# Ninja outputs that mean real work; always-run steps (size checks, the
# bootloader sub-build driver) leave other entries in .ninja_log.
WORK_SUFFIXES = (".obj", ".o", ".a", ".elf", ".map", ".bin", ".ld")
DEFAULT_GLOB = "tests/test_build_components/build/.esphome/build/*"


def _digest(path: Path) -> str | None:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def _snapshot(build_path: Path, name: str) -> dict[str, str | None]:
    files = [*WATCHED, f"build/{name}.elf", f"build/{name}.bin"]
    return {f: _digest(build_path / f) for f in files}


def _ninja_outputs(build_path: Path) -> list[str]:
    log = build_path / "build" / ".ninja_log"
    lines = log.read_text(encoding="utf-8").splitlines() if log.is_file() else []
    return [line.split("\t")[3] for line in lines if not line.startswith("#")]


def _setup_core(build_path: Path, description: dict) -> tuple[str, str]:
    """Point CORE at the tree so ESPHome resolves the same IDF env as the build."""
    from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION, KEY_VARIANT
    import esphome.config_validation as cv
    from esphome.core import CORE

    name = description["project_name"]
    version = Path(description["idf_path"]).name
    CORE.config_path = build_path.parents[2] / f"{name}.yaml"
    CORE.build_path = build_path
    CORE.name = name
    CORE.data[KEY_ESP32] = {
        KEY_IDF_VERSION: cv.Version.parse(version),
        KEY_VARIANT: description["target"].upper(),
    }
    return name, version


def check(build_path: Path) -> list[str]:
    """Return the problems found in one build tree."""
    from esphome.espidf import toolchain

    description = json.loads(
        (build_path / "build" / "project_description.json").read_text(encoding="utf-8")
    )
    name, version = _setup_core(build_path, description)
    env = toolchain._get_idf_env(version)  # pylint: disable=protected-access
    python = toolchain._get_idf_tool("python")  # pylint: disable=protected-access
    idf_py = toolchain._get_idf_path(version) / "tools" / "idf.py"  # pylint: disable=protected-access
    sdkconfig = build_path / f"sdkconfig.{name}"
    sdkconfig_args = ["-D", f"SDKCONFIG={sdkconfig}"] if sdkconfig.is_file() else []

    before = _snapshot(build_path, name)
    outputs_before = len(_ninja_outputs(build_path))
    problems = []
    for action in ("reconfigure", "build"):
        result = subprocess.run(
            [python, str(idf_py), *sdkconfig_args, action],
            cwd=build_path,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            problems.append(f"idf.py {action} failed:\n{result.stdout}{result.stderr}")
            return problems
    after = _snapshot(build_path, name)
    problems += [f"idf.py changed {f}" for f in before if before[f] != after[f]]
    rebuilt = [
        out
        for out in _ninja_outputs(build_path)[outputs_before:]
        if out.endswith(WORK_SUFFIXES)
    ]
    problems += [f"idf.py rebuilt {out}" for out in rebuilt]
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument(
        "build_paths",
        nargs="*",
        type=Path,
        help=f"ESPHome build dirs (default: every native ESP-IDF tree in {DEFAULT_GLOB})",
    )
    parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="exit 0 when there is no native ESP-IDF build tree to check",
    )
    args = parser.parse_args()
    paths = args.build_paths or sorted(REPO_ROOT.glob(DEFAULT_GLOB))
    # Not resolved: SDKCONFIG must be spelled as the build spelled it.
    trees = [
        p
        for p in paths
        if (p / "build" / "project_description.json").is_file()
        and (p / "build" / "CMakeCache.txt").is_file()
    ]
    if not trees:
        print("No native ESP-IDF build tree found")
        return 0 if args.allow_missing else 1

    failed = False
    for tree in trees:
        problems = check(tree)
        print(f"{tree}: {'OK' if not problems else 'DIFFERS'}")
        for problem in problems:
            print(f"  {problem}")
        failed |= bool(problems)
    if failed:
        print(
            "The direct cmake/ninja build no longer matches idf.py. Compare "
            "esphome/espidf/toolchain.py (run_reconfigure, _run_ninja, _size_env) "
            "with the pinned ESP-IDF tools/idf_py_actions."
        )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
