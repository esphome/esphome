#!/usr/bin/env python3
"""Check that a native ESP-IDF build tree is what idf.py itself would produce.

ESPHome runs cmake and ninja directly with the arguments idf.py uses. This
runs the real ``idf.py reconfigure`` and ``idf.py build`` on a finished tree
(in place: CMake rejects a moved cache) and fails if either one changes the
cache, the generated build files or the firmware, or recompiles anything.
It catches drift when the pinned ESP-IDF version changes what idf.py does.
The color and ``size`` environment only change what is printed, so those
parts of the contract are pinned by the unit tests instead.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

WATCHED = (
    "build/CMakeCache.txt",
    "build/build.ninja",
    "build/compile_commands.json",
    "build/project_description.json",
    "build/config/sdkconfig.h",
)
# Only a tree that builds the bootloader has one to watch.
BOOTLOADER_BIN = "build/bootloader/bootloader.bin"
OVERRIDE_INEFFECTIVE = (
    "skip-mode tree built a bootloader; the IDF_BOOTLOADER_OVERRIDE macro in "
    "esphome/build_gen/espidf.py is not taking effect"
)
# Ninja logs whose outputs mean real work when their recorded mtime changes;
# the bootloader is judged by its own sub-build log when one exists.
TOP_NINJA_LOG = "build/.ninja_log"


def _ninja_logs(skip_bootloader: bool) -> list[str]:
    """A missing sub-build log stays an error in the mode that requires one."""
    logs = [TOP_NINJA_LOG]
    if not skip_bootloader:
        logs.append("build/bootloader/.ninja_log")
    return logs


BOOTLOADER_BYPRODUCT = re.compile(r"(^|/build/)bootloader/")
MACRO_CHANGED = (
    "IDF changed __build_process_project_includes; update "
    "IDF_BOOTLOADER_OVERRIDE in esphome/build_gen/espidf.py"
)
VERSION_DRIFT = (
    "ESPHome reads ESP-IDF version {ours!r} from {source} but idf_tools reports "
    "{theirs!r}; update read_idf_version_{source} in esphome/espidf/framework.py"
)
LWIP_NOT_EMPTY = (
    "lwip source {source} compiles to a non-empty object with {option} off; "
    "drop it from LWIP_EMPTY_SOURCES in esphome/build_gen/espidf.py"
)
LWIP_NOTHING_CHECKED = (
    "no lwip object matched LWIP_EMPTY_SOURCES; the build tree layout or the "
    "patterns in esphome/build_gen/espidf.py changed"
)
WORK_SUFFIXES = (".obj", ".o", ".a", ".elf", ".map", ".bin", ".ld")
DEFAULT_GLOB = "tests/test_build_components/build/.esphome/build/*"


def _digest(path: Path) -> str | None:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def watched(name: str, skip_bootloader: bool) -> list[str]:
    """The files idf.py must leave untouched for this tree's mode."""
    files = [*WATCHED, f"build/{name}.elf", f"build/{name}.bin"]
    if not skip_bootloader:
        files.append(BOOTLOADER_BIN)
    return files


def _snapshot(
    build_path: Path, name: str, skip_bootloader: bool
) -> dict[str, str | None]:
    return {f: _digest(build_path / f) for f in watched(name, skip_bootloader)}


def _ninja_mtimes(build_path: Path, logs: list[str]) -> dict[tuple[str, str], str]:
    """(log, output) -> recorded mtime; compaction-safe, unlike a line count."""
    mtimes = {}
    for name in logs:
        log = build_path / name
        lines = log.read_text(encoding="utf-8").splitlines() if log.is_file() else []
        for fields in (line.split("\t") for line in lines if not line.startswith("#")):
            if len(fields) >= 4 and (
                name != TOP_NINJA_LOG or not BOOTLOADER_BYPRODUCT.search(fields[3])
            ):
                mtimes[name, fields[3]] = fields[2]
    return mtimes


def _log_problems(
    build_path: Path, mtimes: dict[tuple[str, str], str], logs: list[str]
) -> list[str]:
    """A missing or unparsable ninja log would otherwise compare as unchanged."""
    problems = []
    for log in logs:
        if not (build_path / log).is_file():
            problems.append(f"missing {log}")
        elif not any(k[0] == log and k[1].endswith(WORK_SUFFIXES) for k in mtimes):
            problems.append(f"no build entries parsed from {log}")
    return problems


def _lwip_empty_source_problems(build_path: Path) -> list[str]:
    """Compile the lwip sources the generated CMakeLists drops; any with
    symbols is a problem. Leaves the tree configured with every source."""
    # pylint: disable=protected-access
    from esphome.build_gen.espidf import LWIP_EMPTY_SOURCES, LWIP_FULL_SOURCES_ENV
    from esphome.espidf import toolchain

    if (rc := toolchain.run_reconfigure(extra_env={LWIP_FULL_SOURCES_ENV: "1"})) != 0:
        return [f"CMake configure with every lwip source failed with exit code {rc}"]
    if rc := toolchain._run_ninja("esp-idf/lwip/liblwip.a", verbose=False, jobs=None):
        return [f"building every lwip source failed with exit code {rc}"]
    build = build_path / "build"
    config = json.loads(
        (build / "config" / "sdkconfig.json").read_text(encoding="utf-8")
    )
    objects = [
        str(obj).removesuffix(".obj")
        for obj in (build / "esp-idf" / "lwip").rglob("*.obj")
    ]
    dropped = [
        (option, source)
        for option, regex in LWIP_EMPTY_SOURCES
        if not config.get(option.removeprefix("CONFIG_"))
        for source in objects
        if re.search(regex, source)
    ]
    if not dropped:
        return [LWIP_NOTHING_CHECKED]
    nm = toolchain._parse_cmakecache(build / "CMakeCache.txt")["CMAKE_NM"]
    return [
        LWIP_NOT_EMPTY.format(source=Path(source).name, option=option)
        for option, source in dropped
        if subprocess.run(
            [nm, "--defined-only", f"{source}.obj"],
            capture_output=True,
            text=True,
            check=False,
        ).stdout.strip()
    ]


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
    # pylint: disable=protected-access
    from esphome.build_gen.espidf import idf_macro_matches
    from esphome.core import CORE
    from esphome.espidf import framework, toolchain

    description = json.loads(
        (build_path / "build" / "project_description.json").read_text(encoding="utf-8")
    )
    name, version = _setup_core(build_path, description)
    # Reconfiguring must not flip the tree's bootloader mode: the check
    # validates the shape the build produced, not this process's flags.
    skip_bootloader = toolchain.tree_skips_bootloader(build_path / "build")
    CORE.skip_bootloader = skip_bootloader
    # A prior tree's memoized decision must not leak into this one.
    toolchain._cache().skip_bootloader = None
    idf_path = toolchain._get_idf_path(version)
    if not idf_macro_matches(idf_path):
        return [MACRO_CHANGED]
    # A managed tree always has version.txt, so the header branch is
    # compared on its own or it would never be exercised here.
    theirs = framework.idf_tools_version(idf_path)
    for source, read in (
        ("txt", framework.read_idf_version_txt),
        ("header", framework.read_idf_version_header),
    ):
        if (ours := read(idf_path)) != theirs:
            return [VERSION_DRIFT.format(ours=ours, source=source, theirs=theirs)]
    # ESP-IDF's openthread stamps the configure time into its compile flags;
    # pin it before the env is cached so both configures get the same value.
    os.environ["SOURCE_DATE_EPOCH"] = "0"
    env = toolchain._get_idf_env(version)
    python = toolchain._get_idf_tool("python")
    idf_py = toolchain._get_idf_path(version) / "tools" / "idf.py"
    sdkconfig = build_path / f"sdkconfig.{name}"
    sdkconfig_args = ["-D", f"SDKCONFIG={sdkconfig}"] if sdkconfig.is_file() else []

    # CMake writes a different build.ninja on a tree's first configure than on
    # a reconfigure, so the baseline is ESPHome's own reconfigure and build.
    if (rc := toolchain.run_reconfigure()) != 0:
        return [f"ESPHome's CMake configure failed with exit code {rc}"]
    if (rc := toolchain._run_ninja("all", verbose=False, jobs=None)) != 0:
        return [f"ESPHome's ninja build failed with exit code {rc}"]
    before = _snapshot(build_path, name, skip_bootloader)
    logs = _ninja_logs(skip_bootloader)
    mtimes_before = _ninja_mtimes(build_path, logs)
    # A moved or renamed output would otherwise compare as "unchanged".
    problems = [f"missing {f}" for f, digest in before.items() if digest is None]
    if skip_bootloader and (build_path / BOOTLOADER_BIN).is_file():
        problems.append(OVERRIDE_INEFFECTIVE)
    if problems := problems + _log_problems(build_path, mtimes_before, logs):
        return problems
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
            return [f"idf.py {action} failed:\n{result.stdout}{result.stderr}"]
    after = _snapshot(build_path, name, skip_bootloader)
    mtimes_after = _ninja_mtimes(build_path, logs)
    problems = [f"idf.py changed {f}" for f in before if before[f] != after[f]]
    problems += _log_problems(build_path, mtimes_after, logs)
    for key in sorted(mtimes_before.keys() | mtimes_after.keys()):
        log, out = key
        if not out.endswith(WORK_SUFFIXES):
            continue
        if key not in mtimes_after:
            problems.append(f"idf.py dropped {out} from {log}")
        elif mtimes_before.get(key) != mtimes_after[key]:
            problems.append(f"idf.py rebuilt {out}")
    # Last: it reconfigures the tree, which would otherwise relink above.
    return problems or _lwip_empty_source_problems(build_path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument(
        "build_paths",
        nargs="*",
        type=Path,
        help=f"ESPHome build dirs (default: the first native ESP-IDF tree in {DEFAULT_GLOB})",
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
    if rejected := [p for p in args.build_paths if p not in trees]:
        for path in rejected:
            print(f"{path}: not a configured native ESP-IDF build tree")
        return 1
    if not trees:
        print("No native ESP-IDF build tree found")
        return 1
    if not args.build_paths:
        # The contract does not depend on the target, so one tree is enough.
        trees = trees[:1]

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
            "_IdfPyContract and its users in esphome/espidf/toolchain.py with the "
            "pinned ESP-IDF tools/idf_py_actions."
        )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
