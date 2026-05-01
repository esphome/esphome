from __future__ import annotations

import logging
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request

from esphome.core import CORE
from esphome.helpers import rmtree

_LOGGER = logging.getLogger(__name__)

PathType = str | os.PathLike

_WEST_VERSION = "1.5.0"
_NINJA_VERSION = "1.11.1.1"
_TOOLCHAIN_VERSION = "0.17.4"
_TOOLCHAIN_BASE_URL = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download"


def _get_pythonexe_path() -> str:
    return os.environ.get("PYTHONEXEPATH", os.path.normpath(sys.executable))


def _create_venv(root: PathType, msg: str | None = None):
    cmd = [_get_pythonexe_path(), "-m", "venv", "--clear", root]
    if not _exec_ok(cmd, msg=f"Create Python virtual environment for {msg}"):
        raise RuntimeError(f"Can't create Python virtual environment for {msg}")


def _get_zephyr_tools_path() -> Path:
    return CORE.data_dir / "zephyr"


def _exec_ok(*args, **kwargs) -> bool:
    return _exec(*args, **kwargs)[0]


def _exec(
    cmd: list[str],
    msg: str | None = None,
    env: dict[str, str] | None = None,
    cwd: PathType | None = None,
) -> tuple[bool, str | None, str | None]:
    cmd_str = msg or " ".join(cmd)
    try:
        _LOGGER.debug("%s - running ...", cmd_str)

        run_env = {**os.environ, **env} if env else None

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False,
            env=run_env,
            cwd=cwd,
        )

        if result.returncode != 0:
            tail = (result.stderr or result.stdout or "").strip()[-1000:]
            _LOGGER.error(
                "%s - failed (returncode=%s). Tail:\n%s",
                cmd_str,
                result.returncode,
                tail,
            )
            return False, result.stdout, result.stderr

        _LOGGER.debug("%s - executed successfully", cmd_str)
        return True, result.stdout, result.stderr

    except (subprocess.SubprocessError, OSError) as e:
        _LOGGER.error("%s - error: %s", cmd_str, str(e))
        return False, None, None


def _get_python_env_path(version: str) -> Path:
    return _get_zephyr_tools_path() / "penvs" / version


def _venv_bin(venv_path: Path, name: str) -> Path:
    if sys.platform == "win32":
        return venv_path / "Scripts" / f"{name}.exe"
    return venv_path / "bin" / name


def _ensure_west(venv_path: Path) -> None:
    if _venv_bin(venv_path, "west").exists():
        return
    _LOGGER.info("Installing west %s ...", _WEST_VERSION)
    _create_venv(venv_path, "west")
    pip = _venv_bin(venv_path, "pip")
    if not _exec_ok(
        [str(pip), "install", f"west=={_WEST_VERSION}", f"ninja=={_NINJA_VERSION}"],
        msg=f"Install west {_WEST_VERSION}, ninja {_NINJA_VERSION}",
    ):
        raise RuntimeError(f"Can't install west {_WEST_VERSION}")


def _ensure_nrf_sdk(venv_path: Path, framework_ver) -> Path:
    nrf_version = f"v{framework_ver.major}.{framework_ver.minor}.{framework_ver.patch}"
    sdk_path = _get_zephyr_tools_path() / "ncs" / nrf_version
    sentinel = sdk_path / ".esphome_complete"

    if sentinel.exists():
        _LOGGER.debug("nRF Connect SDK %s already at %s", nrf_version, sdk_path)
        return sdk_path

    if sdk_path.exists():
        _LOGGER.warning("Incomplete nRF Connect SDK %s — cleaning up", nrf_version)
        rmtree(sdk_path)

    _LOGGER.info("Initializing nRF Connect SDK %s ...", nrf_version)
    sdk_path.mkdir(parents=True, exist_ok=True)

    west = _venv_bin(venv_path, "west")
    if not _exec_ok(
        [
            str(west),
            "init",
            "-m",
            "https://github.com/nrfconnect/sdk-nrf",
            "--mr",
            nrf_version,
            str(sdk_path),
        ],
        msg=f"west init nRF Connect SDK {nrf_version}",
    ):
        shutil.rmtree(sdk_path, ignore_errors=True)
        raise RuntimeError(f"Can't initialize nRF Connect SDK {nrf_version}")

    _LOGGER.info("Updating nRF Connect SDK %s (this may take a while) ...", nrf_version)
    if not _exec_ok(
        [str(west), "update", "--narrow", "--fetch-opt=--depth=1"],
        msg=f"west update nRF Connect SDK {nrf_version}",
        cwd=sdk_path,
    ):
        shutil.rmtree(sdk_path, ignore_errors=True)
        raise RuntimeError(f"Can't update nRF Connect SDK {nrf_version}")

    python = _venv_bin(venv_path, "python")
    _exec_ok(
        [
            str(python),
            "-m",
            "pip",
            "install",
            "-r",
            str(sdk_path / "zephyr/scripts/requirements.txt"),
            "-r",
            str(sdk_path / "nrf/scripts/requirements.txt"),
        ],
        msg="Install Python requirements",
    )

    sentinel.touch()
    return sdk_path


def _ensure_toolchain(version: str) -> Path:
    toolchains_dir = _get_zephyr_tools_path() / "toolchains"
    toolchain_path = toolchains_dir / f"zephyr-sdk-{version}"
    sentinel = toolchain_path / ".esphome_complete"

    if sentinel.exists():
        _LOGGER.debug("Zephyr SDK toolchain v%s already installed", version)
        return toolchain_path

    if toolchain_path.exists():
        _LOGGER.warning("Incomplete Zephyr SDK toolchain v%s — cleaning up", version)
        rmtree(toolchain_path)

    machine = platform.machine().lower()
    if machine in ("x86_64", "amd64"):
        arch = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch = "aarch64"
    else:
        raise RuntimeError(f"Unsupported CPU architecture: {machine}")

    if sys.platform.startswith("linux"):
        os_tag = "linux"
    elif sys.platform == "darwin":
        os_tag = "macos"
    else:
        raise RuntimeError(f"Unsupported OS for Zephyr toolchain: {sys.platform}")

    filename = f"zephyr-sdk-{version}_{os_tag}-{arch}.tar.xz"
    url = f"{_TOOLCHAIN_BASE_URL}/v{version}/{filename}"
    toolchains_dir.mkdir(parents=True, exist_ok=True)
    archive_tmp = toolchains_dir / f"{filename}.tmp"

    _LOGGER.info("Downloading Zephyr SDK toolchain v%s ...", version)
    try:
        urllib.request.urlretrieve(url, archive_tmp)
    except Exception as e:
        archive_tmp.unlink(missing_ok=True)
        raise RuntimeError(
            f"Can't download Zephyr SDK toolchain v{version}: {e}"
        ) from e

    _LOGGER.info("Extracting Zephyr SDK toolchain v%s ...", version)
    extract_tmp = toolchains_dir / f"zephyr-sdk-{version}.tmp"
    try:
        extract_tmp.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive_tmp, "r:xz") as tar:
            tar.extractall(extract_tmp)
        entries = list(extract_tmp.iterdir())
        if len(entries) != 1 or not entries[0].is_dir():
            raise RuntimeError(
                f"Unexpected archive layout in Zephyr SDK toolchain v{version}"
            )
        entries[0].rename(toolchain_path)
    except Exception as e:
        shutil.rmtree(extract_tmp, ignore_errors=True)
        shutil.rmtree(toolchain_path, ignore_errors=True)
        raise RuntimeError(f"Can't extract Zephyr SDK toolchain v{version}: {e}") from e
    finally:
        archive_tmp.unlink(missing_ok=True)
        shutil.rmtree(extract_tmp, ignore_errors=True)

    setup_script = toolchain_path / "setup.sh"
    if setup_script.exists() and not _exec_ok(
        [str(setup_script), "-t", "arm-zephyr-eabi", "-h"],
        msg=f"Setup Zephyr SDK toolchain v{version}",
    ):
        _LOGGER.warning(
            "Zephyr SDK toolchain setup.sh returned an error; "
            "the toolchain may still work"
        )

    sentinel.touch()
    return toolchain_path


def check_and_install() -> tuple[Path, Path, Path]:
    """Install west, nRF Connect SDK, and Zephyr toolchain.

    Returns (venv_path, sdk_path, toolchain_path).
    """
    from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION

    framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    nrf_version = f"v{framework_ver.major}.{framework_ver.minor}.{framework_ver.patch}"
    venv_path = _get_python_env_path(f"west-{_WEST_VERSION}_ncs-{nrf_version}")
    _ensure_west(venv_path)
    sdk_path = _ensure_nrf_sdk(venv_path, framework_ver)
    toolchain_path = _ensure_toolchain(_TOOLCHAIN_VERSION)
    return venv_path, sdk_path, toolchain_path
