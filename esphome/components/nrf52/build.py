"""Native west/Zephyr build for nRF52."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import shutil
import subprocess
import sys

from esphome.core import CORE, EsphomeError
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)


def _cmake_lists_content() -> str:
    compile_defs = sorted(
        flag[2:] for flag in CORE.build_flags if flag.startswith("-D")
    )
    def_lines = "\n    ".join(f'"{d}"' for d in compile_defs)
    return f"""\
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{{ZEPHYR_BASE}})
project(esphome)
file(GLOB_RECURSE APP_SOURCES src/*.cpp src/*.c)
target_sources(app PRIVATE ${{APP_SOURCES}})
target_include_directories(app PRIVATE src)
target_compile_definitions(app PRIVATE
    {def_lines}
)
"""


def write_sysbuild_cmake(python_bin: Path) -> None:
    """Write sysbuild.cmake that forwards Python3_EXECUTABLE to child image cmake."""
    content = f"""\
set_property(
    TARGET ${{DEFAULT_IMAGE}}
    APPEND PROPERTY IMAGE_CMAKE_ARGS
    "-DPython3_EXECUTABLE={python_bin}"
)
"""
    write_file_if_changed(CORE.relative_build_path("sysbuild.cmake"), content)


def write_cmake_lists() -> None:
    write_file_if_changed(
        CORE.relative_build_path("CMakeLists.txt"),
        _cmake_lists_content(),
    )


def _has_stale_python(build_dir: Path, expected: Path) -> bool:
    """Return True if any CMakeCache.txt in build_dir has a different Python3_EXECUTABLE."""
    for cache in build_dir.glob("**/CMakeCache.txt"):
        try:
            for line in cache.read_text(encoding="utf-8").splitlines():
                if line.startswith("Python3_EXECUTABLE:FILEPATH="):
                    cached = Path(line.split("=", 1)[1].strip())
                    if cached.resolve() != expected.resolve():
                        return True
        except OSError:
            pass
    return False


def _find_ninja(venv_ninja: Path) -> Path:
    system = shutil.which("ninja")
    if system:
        return Path(system)
    if venv_ninja.exists():
        return venv_ninja
    raise EsphomeError(
        "ninja build tool not found. Install it with:\n"
        "  sudo apt install ninja-build   # Debian/Ubuntu\n"
        "  brew install ninja             # macOS"
    )


def run_west_build(
    venv_path: Path,
    sdk_path: Path,
    toolchain_path: Path,
    board: str,
    verbose: bool,
    second_bootloader: bool = False,
) -> int:
    app_dir = CORE.build_path
    build_dir = CORE.relative_build_path(
        ".west_build_sysbuild" if second_bootloader else ".west_build"
    )
    zephyr_base = sdk_path / "zephyr"

    scripts = "Scripts" if sys.platform == "win32" else "bin"
    suffix = ".exe" if sys.platform == "win32" else ""
    west_bin = venv_path / scripts / f"west{suffix}"
    ninja_bin = _find_ninja(venv_path / scripts / f"ninja{suffix}")
    python_bin = venv_path / scripts / f"python{suffix}"

    # Add the west venv's site-packages to PYTHONPATH so that Zephyr scripts
    # (e.g. snippets.py) can import packages like pykwalify regardless of which
    # Python cmake's find_package(Python3) resolves to in child cmake invocations.
    site_packages = [
        str(sp)
        for sp in (venv_path / "lib").glob("python*/site-packages")
        if sp.is_dir()
    ]
    existing_pythonpath = os.environ.get("PYTHONPATH", "")
    pythonpath_parts = site_packages + (
        [existing_pythonpath] if existing_pythonpath else []
    )

    env = {
        **os.environ,
        "PATH": str(venv_path / scripts) + os.pathsep + os.environ.get("PATH", ""),
        "PYTHONPATH": os.pathsep.join(pythonpath_parts),
        "ZEPHYR_BASE": str(zephyr_base),
        "ZEPHYR_SDK_INSTALL_DIR": str(toolchain_path),
        "ZEPHYR_TOOLCHAIN_VARIANT": "zephyr",
    }

    if second_bootloader:
        write_sysbuild_cmake(python_bin)

    if build_dir.exists() and _has_stale_python(build_dir, python_bin):
        _LOGGER.info("Cleaning west build dir: Python3 changed (%s)", build_dir)
        shutil.rmtree(build_dir)

    cmake_args = [
        f"-DCMAKE_MAKE_PROGRAM={ninja_bin}",
        f"-DPython3_EXECUTABLE={python_bin}",
        f"-DCONF_FILE={app_dir / 'zephyr' / 'prj.conf'}",
        f"-DDTC_OVERLAY_FILE={app_dir / 'zephyr' / 'app.overlay'}",
    ]
    pm_static = app_dir / "zephyr" / "pm_static.yml"
    if pm_static.exists():
        cmake_args.append(f"-DPM_STATIC_YML_FILE={pm_static}")

    cmd = [
        str(west_bin),
        "build",
        *(["--sysbuild"] if second_bootloader else []),
        "--board",
        board,
        "--build-dir",
        str(build_dir),
        str(app_dir),
        "--",
        *cmake_args,
    ]

    _LOGGER.info("Running west build for board %s ...", board)
    _LOGGER.debug("Command: %s", " ".join(cmd))
    _LOGGER.debug("ZEPHYR_BASE: %s", zephyr_base)

    result = subprocess.run(cmd, env=env, check=False)
    return result.returncode
