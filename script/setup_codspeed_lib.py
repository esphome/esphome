#!/usr/bin/env python3
"""Set up CodSpeed's google_benchmark fork as a PlatformIO library.

CodSpeed requires their codspeed-cpp fork for CPU simulation instrumentation.
This script clones the repo and assembles a flat PlatformIO-compatible library
by combining google_benchmark sources, codspeed core, and instrument-hooks.

PlatformIO quirks addressed:
  - .cc files renamed to .cpp (PlatformIO ignores .cc)
  - All sources merged into one src/ dir (PlatformIO can't compile from
    multiple source directories in a single library)
  - library.json created with required CodSpeed preprocessor defines

Usage:
    python script/setup_codspeed_lib.py [--output-dir DIR]

Prints JSON to stdout with lib_path for cpp_benchmark.py.
Git output goes to stderr.

See https://codspeed.io/docs/benchmarks/cpp#custom-build-systems
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

# Pin to a specific release for reproducibility
CODSPEED_CPP_REPO = "https://github.com/CodSpeedHQ/codspeed-cpp.git"
CODSPEED_CPP_SHA = "e633aca00da3d0ad14e7bf424d9cb47165a29028"  # v2.1.0

DEFAULT_OUTPUT_DIR = "/tmp/codspeed-cpp"

# Well-known paths within the codspeed-cpp repository
GOOGLE_BENCHMARK_SUBDIR = "google_benchmark"
CORE_SUBDIR = "core"
INSTRUMENT_HOOKS_SUBDIR = Path(CORE_SUBDIR) / "instrument-hooks"
INSTRUMENT_HOOKS_INCLUDES = INSTRUMENT_HOOKS_SUBDIR / "includes"
INSTRUMENT_HOOKS_DIST = INSTRUMENT_HOOKS_SUBDIR / "dist" / "core.c"
CORE_CMAKE = Path(CORE_SUBDIR) / "CMakeLists.txt"


def _git(args: list[str], **kwargs: object) -> None:
    """Run a git command, sending output to stderr."""
    subprocess.run(
        ["git", *args],
        check=True,
        stdout=kwargs.pop("stdout", sys.stderr),
        stderr=kwargs.pop("stderr", sys.stderr),
        **kwargs,
    )


def _clone_repo(output_dir: Path) -> None:
    """Shallow-clone codspeed-cpp at the pinned SHA with submodules."""
    output_dir.mkdir(parents=True, exist_ok=True)
    _git(["init", str(output_dir)])
    _git(["-C", str(output_dir), "remote", "add", "origin", CODSPEED_CPP_REPO])
    _git(["-C", str(output_dir), "fetch", "--depth", "1", "origin", CODSPEED_CPP_SHA])
    _git(
        ["-C", str(output_dir), "checkout", "FETCH_HEAD"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    _git(
        [
            "-C",
            str(output_dir),
            "submodule",
            "update",
            "--init",
            "--recursive",
            "--depth",
            "1",
        ]
    )


def _read_codspeed_version(cmake_path: Path) -> str:
    """Extract CODSPEED_VERSION from core/CMakeLists.txt."""
    if not cmake_path.exists():
        return "0.0.0"
    for line in cmake_path.read_text().splitlines():
        if line.startswith("set(CODSPEED_VERSION"):
            return line.split()[1].rstrip(")")
    return "0.0.0"


def _rename_cc_to_cpp(src_dir: Path) -> None:
    """Rename .cc files to .cpp so PlatformIO compiles them."""
    for cc_file in src_dir.glob("*.cc"):
        cpp_file = cc_file.with_suffix(".cpp")
        if not cpp_file.exists():
            cc_file.rename(cpp_file)


def _copy_if_missing(src: Path, dest: Path) -> None:
    """Copy a file only if the destination doesn't already exist."""
    if not dest.exists():
        shutil.copy2(src, dest)


def _merge_codspeed_core_into_lib(core_src: Path, lib_src: Path) -> None:
    """Copy codspeed core sources into the benchmark library src/.

    .cpp files get a ``codspeed_`` prefix to avoid name collisions with
    google_benchmark's own sources.  .h files keep their original names
    since they're referenced by ``#include "walltime.h"`` etc.
    """
    for src_file in core_src.iterdir():
        if src_file.suffix == ".cpp":
            _copy_if_missing(src_file, lib_src / f"codspeed_{src_file.name}")
        elif src_file.suffix == ".h":
            _copy_if_missing(src_file, lib_src / src_file.name)


def _write_library_json(
    benchmark_dir: Path,
    core_include: Path,
    hooks_include: Path,
    version: str,
    project_root: Path,
) -> None:
    """Write a PlatformIO library.json with CodSpeed build flags."""
    library_json = {
        "name": "benchmark",
        "version": "0.0.0",
        "build": {
            "flags": [
                f"-I{core_include}",
                f"-I{hooks_include}",
                # google benchmark build flags
                # -O2 is critical: without it, instrument_hooks_start_benchmark_inline
                # doesn't get inlined and shows up as overhead in profiles
                "-O2",
                "-DNDEBUG",
                "-DHAVE_STD_REGEX",
                "-DHAVE_STEADY_CLOCK",
                "-DBENCHMARK_STATIC_DEFINE",
                # CodSpeed instrumentation flags
                # https://codspeed.io/docs/benchmarks/cpp#custom-build-systems
                "-DCODSPEED_ENABLED",
                "-DCODSPEED_ANALYSIS",
                f'-DCODSPEED_VERSION=\\"{version}\\"',
                f'-DCODSPEED_ROOT_DIR=\\"{project_root}\\"',
                '-DCODSPEED_MODE_DISPLAY=\\"simulation\\"',
            ],
            "includeDir": "include",
        },
    }
    (benchmark_dir / "library.json").write_text(
        json.dumps(library_json, indent=2) + "\n"
    )


def setup_codspeed_lib(output_dir: Path) -> None:
    """Clone codspeed-cpp and assemble a flat PlatformIO library.

    The resulting library at ``output_dir/google_benchmark/`` contains:
      - google_benchmark sources (.cc renamed to .cpp)
      - codspeed core sources (prefixed ``codspeed_``)
      - instrument-hooks C source (as ``instrument_hooks.c``)
      - library.json with all required CodSpeed defines

    Args:
        output_dir: Directory to clone the repository into
    """
    if not (output_dir / ".git").exists():
        _clone_repo(output_dir)
    else:
        # Verify the existing checkout matches the pinned SHA
        result = subprocess.run(
            ["git", "-C", str(output_dir), "rev-parse", "HEAD"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0 or result.stdout.strip() != CODSPEED_CPP_SHA:
            print(
                f"Stale codspeed-cpp checkout, re-cloning at {CODSPEED_CPP_SHA}",
                file=sys.stderr,
            )
            shutil.rmtree(output_dir)
            _clone_repo(output_dir)

    benchmark_dir = output_dir / GOOGLE_BENCHMARK_SUBDIR
    lib_src = benchmark_dir / "src"
    core_dir = output_dir / CORE_SUBDIR
    core_include = core_dir / "include"
    hooks_include = output_dir / INSTRUMENT_HOOKS_INCLUDES
    hooks_dist_c = output_dir / INSTRUMENT_HOOKS_DIST
    project_root = Path(__file__).resolve().parent.parent

    # 1. Rename .cc → .cpp (PlatformIO doesn't compile .cc)
    _rename_cc_to_cpp(lib_src)

    # 2. Merge codspeed core sources into the library
    _merge_codspeed_core_into_lib(core_dir / "src", lib_src)

    # 3. Copy instrument-hooks C source (provides instrument_hooks_* symbols)
    if hooks_dist_c.exists():
        _copy_if_missing(hooks_dist_c, lib_src / "instrument_hooks.c")

    # 4. Write library.json
    version = _read_codspeed_version(output_dir / CORE_CMAKE)
    _write_library_json(
        benchmark_dir, core_include, hooks_include, version, project_root
    )

    # Output JSON config for cpp_benchmark.py
    print(json.dumps({"lib_path": str(benchmark_dir)}))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(DEFAULT_OUTPUT_DIR),
        help=f"Directory to clone codspeed-cpp into (default: {DEFAULT_OUTPUT_DIR})",
    )
    args = parser.parse_args()
    setup_codspeed_lib(args.output_dir)


if __name__ == "__main__":
    main()
