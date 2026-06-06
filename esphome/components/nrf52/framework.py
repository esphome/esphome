import logging
import os
from pathlib import Path
import platform
import tempfile

from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    archive_extract_all,
    create_venv,
    download_from_mirrors,
    get_python_env_executable_path,
    rmdir,
    run_command_ok,
    str_to_lst_of_str,
)

_LOGGER = logging.getLogger(__name__)

_WEST_VERSION = "1.5.0"
_TOOLCHAIN_VERSION = "0.17.4"

SDK_NG_TOOLCHAIN_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_TOOLCHAIN_MIRRORS",
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/toolchain_{sysname}-{machine}_arm-zephyr-eabi.{extension}",
    )
)


def _get_tools_path() -> Path:
    return CORE.data_dir / "sdk-nrf"


def _get_python_env_path(version: str) -> Path:
    return _get_tools_path() / "penvs" / version


def _get_framework_path(version: str) -> Path:
    return _get_tools_path() / "frameworks" / f"{version}"


def _get_toolchain_path(version: str) -> Path:
    return _get_tools_path() / "toolchains" / f"{version}"


def _get_toolchain_platform_info() -> tuple[str, str, str]:
    """Return (sysname, machine, extension) for the current host."""
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
    return sysname, machine, extension


def check_and_install() -> None:
    framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    version = f"v{framework_ver.major}.{framework_ver.minor}.{framework_ver.patch}"
    python_env_path = _get_python_env_path(version)
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    sentinel = python_env_path / ".ready"
    install_venv = not sentinel.exists()
    if install_venv:
        rmdir(python_env_path, msg=f"Clean up {version} Python environment")

        create_venv(python_env_path, msg=f"{version}")

        _LOGGER.info("Installing west %s ...", _WEST_VERSION)
        cmd = [str(env_python_path), "-m", "pip", "install", f"west=={_WEST_VERSION}"]
        if not run_command_ok(
            cmd,
        ):
            raise EsphomeError(f"Install west for {version} Python environment failure")
        sentinel.touch()

    framework_path = _get_framework_path(version)
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
        if not run_command_ok(
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
        if not run_command_ok(cmd, cwd=framework_path):
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

            sysname, machine, extension = _get_toolchain_platform_info()

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
