import logging
import os
from pathlib import Path
import platform
import tempfile

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_BOARD
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError
from esphome.espidf.framework import (
    _create_venv,
    _exec_ok,
    _get_python_env_executable_path,
    _str_to_lst_of_str,
    archive_extract_all,
    download_from_mirrors,
    rmdir,
)

_LOGGER = logging.getLogger(__name__)

_TOOLCHAIN_VERSION = "0.17.4"

SDK_NG_TOOLCHAIN_MIRRORS = _str_to_lst_of_str(
    "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/toolchain_{sysname}-{machine}_arm-zephyr-eabi.{extension}",
)


def _get_tools_path() -> Path:
    return CORE.data_dir / "sdk-nrf"


def _get_python_env_path(version: str) -> Path:
    return _get_tools_path() / "penvs" / version


def _get_framework_path(version: str) -> Path:
    return _get_tools_path() / "frameworks" / f"{version}"


def _get_toolchain_path(version: str) -> Path:
    return _get_tools_path() / "toolchains" / f"{version}"


def _get_requirements_path() -> Path:
    return Path(__file__).parent / "requirements.txt"


def check_and_install() -> (Path, dict[str, str]):
    framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    version = f"v{framework_ver.major}.{framework_ver.minor}.{framework_ver.patch}"
    python_env_path = _get_python_env_path(version)
    env_python_path = _get_python_env_executable_path(python_env_path, "python")
    framework_path = _get_framework_path(version)
    west_env = {
        **os.environ,
        "ZEPHYR_BASE": str(framework_path),
        "PATH": f"{env_python_path.parent}{os.pathsep}{os.environ.get('PATH', '')}",
    }
    print(west_env)

    venv_sentinel = python_env_path / ".ready"
    requirements_path = _get_requirements_path()

    install_venv = not env_python_path.exists()
    requirements_changed = (
        not venv_sentinel.exists()
        or venv_sentinel.stat().st_mtime < requirements_path.stat().st_mtime
    )

    if install_venv:
        rmdir(python_env_path, msg=f"Clean up {version} Python environment")
        _create_venv(python_env_path, msg=f"{version}")

    if install_venv or requirements_changed:
        _LOGGER.info("Installing Python packages from %s ...", requirements_path.name)
        cmd = [
            str(env_python_path),
            "-m",
            "pip",
            "install",
            "--upgrade",
            "-r",
            str(requirements_path),
        ]
        if not _exec_ok(cmd):
            raise EsphomeError(f"Upgrade {version} Python environment packages failure")
        venv_sentinel.touch()

    sentinel = framework_path / ".ready"
    if install_venv or not sentinel.exists():
        rmdir(framework_path, msg=f"Clean up {version} framework environment")
        _LOGGER.info("Initializing nRF Connect SDK %s ...", version)
        cmd = [
            str(env_python_path),
            "-m",
            "west",
            "init",
            "-m",
            "https://github.com/nrfconnect/sdk-nrf",
            "--mr",
            f"{version}",
            str(framework_path),
        ]
        if not _exec_ok(
            cmd,
        ):
            raise EsphomeError(f"Can't initialize nRF Connect SDK {version}")
        _LOGGER.info("Updating nRF Connect SDK %s (this may take a while) ...", version)
        cmd = [
            str(env_python_path),
            "-m",
            "west",
            "update",
            "--narrow",
            "--fetch-opt=--depth=1",
        ]
        if not _exec_ok(cmd, env=west_env):
            raise EsphomeError(f"Can't update nRF Connect SDK {version}")
        sentinel.touch()

    toolchains_dir = _get_toolchain_path(_TOOLCHAIN_VERSION)
    sentinel = toolchains_dir / ".ready"
    if not sentinel.exists():
        rmdir(
            toolchains_dir, msg=f"Clean up {_TOOLCHAIN_VERSION} toolchain environment"
        )
        with tempfile.NamedTemporaryFile() as tmp:
            _LOGGER.info("Downloading %s toolchain ...", _TOOLCHAIN_VERSION)

            extension = "tar.xz"
            sysname = platform.system().lower()
            machine = platform.machine()
            if machine == "arm64":
                machine = "aarch64"
            if sysname == "darwin":
                sysname = "macos"
            elif sysname == "windows":
                machine = "x86_64"
                extension = "7z"

            download_from_mirrors(
                SDK_NG_TOOLCHAIN_MIRRORS,
                {
                    "VERSION": _TOOLCHAIN_VERSION,
                    "sysname": sysname,
                    "machine": machine,
                    "extension": extension,
                },
                tmp.file,
            )
            archive_extract_all(tmp.file, toolchains_dir, progress_header="Extracting")
        sentinel.touch()

    return (env_python_path, west_env)


def get_component_cmakelists() -> str:
    # Extract compile options (-W flags, excluding linker flags)
    compile_opts = [
        flag
        for flag in CORE.build_flags
        if flag.startswith("-W") and not flag.startswith("-Wl,")
    ]
    compile_opts_str = "\n    ".join(sorted(compile_opts)) if compile_opts else ""

    # Extract linker options (-Wl, flags)
    link_opts = [flag for flag in CORE.build_flags if flag.startswith("-Wl,")]
    link_opts_str = "\n    ".join(sorted(link_opts)) if link_opts else ""

    return """\
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(esphome)

file(GLOB_RECURSE APP_SOURCES *.cpp *.c)
target_sources(app PRIVATE ${APP_SOURCES})
"""


# SET(CMAKE_CXX_FLAGS  "${{CMAKE_CXX_FLAGS}} {build_flags}")
# SET(CMAKE_C_FLAGS  "${{CMAKE_C_FLAGS}} {build_flags}")
# zephyr_ld_options({link_flags})

# target_sources(app PRIVATE {" ".join(source_files)})
# target_include_directories(app PRIVATE ../src)
# target_link_options(app INTERFACE {link_flags})


# target_include_directories(app PRIVATE src)
# target_compile_definitions(app PRIVATE
#     {def_lines}
# )


def build(env_python_path: Path, west_env: dict[str, str]) -> None:
    _LOGGER.info("Building ...")
    cmd = [
        str(env_python_path),
        "-m",
        "west",
        "build",
        "--board",
        zephyr_data()[KEY_BOARD],
        str(CORE.relative_src_path()),
    ]
    if not _exec_ok(
        cmd,
        env=west_env,
    ):
        raise EsphomeError("Build failed")
