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

_REQUIREMENTS = Path(__file__).parent / "requirements.txt"
_TOOLCHAIN_VERSION = "0.17.4"

SDK_NG_TOOLCHAIN_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_TOOLCHAIN_MIRRORS",
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/toolchain_{sysname}-{machine}_arm-zephyr-eabi.{extension}",
    )
)

# Minimal SDK provides cmake discovery files (Zephyr-sdkConfig.cmake) and
# host tools (dtc etc.) required by the Zephyr cmake build system.
SDK_NG_MINIMAL_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_MINIMAL_MIRRORS",
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/zephyr-sdk-{VERSION}_{sysname}-{machine}_minimal.{extension}",
    )
)


def _get_tools_path() -> Path:
    return CORE.data_dir / "sdk-nrf"


def _get_python_env_path(version: str) -> Path:
    return _get_tools_path() / "penvs" / version


def _get_framework_path(version: str) -> Path:
    return _get_tools_path() / "frameworks" / version


def _get_toolchain_path(version: str) -> Path:
    return _get_tools_path() / "toolchains" / version


_SITECUSTOMIZE = """\
import os, stat, shutil
_orig = shutil.rmtree
def _handler(func, path, exc):
    os.chmod(path, stat.S_IWRITE); func(path)
def _rmtree(path, ignore_errors=False, onerror=None, *, onexc=None, dir_fd=None):
    if onerror is None and onexc is None:
        onexc = _handler
    return _orig(path, ignore_errors=ignore_errors, onerror=onerror, onexc=onexc, dir_fd=dir_fd)
shutil.rmtree = _rmtree
"""


def _install_sitecustomize(python_env_path: Path) -> None:
    """Patch shutil.rmtree inside the penv to handle read-only files.

    west init's shutil.move falls back to copytree+rmtree on Windows, and
    rmtree dies on the read-only .idx/.pack files git just wrote into
    manifest-tmp. Dropping a sitecustomize.py into the venv applies the
    same fix esphome.helpers.rmtree uses, but inside the subprocess.
    """
    if os.name != "nt":
        return
    site_packages = python_env_path / "Lib" / "site-packages"
    site_packages.mkdir(parents=True, exist_ok=True)
    (site_packages / "sitecustomize.py").write_text(_SITECUSTOMIZE, encoding="utf-8")


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


def _get_version_str() -> str:
    framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    return f"v{framework_ver.major}.{framework_ver.minor}.{framework_ver.patch}"


def get_build_paths() -> dict:
    version = _get_version_str()
    env_path = _get_python_env_path(version)
    return {
        "python_executable": get_python_env_executable_path(env_path, "python"),
        "framework_path": _get_framework_path(version),
    }


def get_build_env() -> dict:
    version = _get_version_str()
    venv_bin_dir = get_python_env_executable_path(
        _get_python_env_path(version), "python"
    ).parent
    env = os.environ.copy()
    env["PATH"] = str(venv_bin_dir) + os.pathsep + env.get("PATH", "")
    env["ZEPHYR_BASE"] = str(_get_framework_path(version) / "zephyr")
    env["Zephyr-sdk_DIR"] = str(_get_toolchain_path(_TOOLCHAIN_VERSION) / "cmake")
    return env


def check_and_install() -> None:
    version = _get_version_str()
    python_env_path = _get_python_env_path(version)
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    sentinel = python_env_path / ".ready"
    install_venv = (
        not sentinel.exists()
        or _REQUIREMENTS.stat().st_mtime > sentinel.stat().st_mtime
    )
    if install_venv:
        rmdir(python_env_path, msg=f"Clean up {version} Python environment")

        create_venv(python_env_path, msg=version)

        _install_sitecustomize(python_env_path)

        _LOGGER.info("Installing requirements ...")
        cmd = [
            str(env_python_path),
            "-m",
            "pip",
            "install",
            "-r",
            str(_REQUIREMENTS),
        ]
        if not run_command_ok(cmd):
            raise EsphomeError(
                f"Install requirements for {version} Python environment failure"
            )
        sentinel.touch()

    framework_path = _get_framework_path(version)
    sentinel = framework_path / ".ready"
    zephyr_reqs = framework_path / "zephyr" / "scripts" / "requirements.txt"
    if not sentinel.exists() or not zephyr_reqs.exists():
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
            version,
            str(framework_path),
        ]
        if not run_command_ok(cmd):
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

    zephyr_sentinel = python_env_path / ".zephyr_reqs_ready"
    if (
        install_venv
        or not zephyr_sentinel.exists()
        or zephyr_reqs.stat().st_mtime > zephyr_sentinel.stat().st_mtime
    ):
        _LOGGER.info("Installing Zephyr requirements ...")
        cmd = [
            str(env_python_path),
            "-m",
            "pip",
            "install",
            "-r",
            str(zephyr_reqs),
        ]
        if not run_command_ok(cmd):
            raise EsphomeError(f"Install Zephyr requirements for {version} failure")
        zephyr_sentinel.touch()

    toolchains_dir = _get_toolchain_path(_TOOLCHAIN_VERSION)
    sentinel = toolchains_dir / ".ready"
    if not sentinel.exists():
        rmdir(
            toolchains_dir, msg=f"Clean up {_TOOLCHAIN_VERSION} toolchain environment"
        )
        sysname, machine, extension = _get_toolchain_platform_info()
        with tempfile.NamedTemporaryFile() as tmp:
            _LOGGER.info("Downloading Zephyr SDK %s minimal ...", _TOOLCHAIN_VERSION)
            download_from_mirrors(
                SDK_NG_MINIMAL_MIRRORS,
                {
                    "VERSION": _TOOLCHAIN_VERSION,
                    "sysname": sysname,
                    "machine": machine,
                    "extension": extension,
                },
                tmp.file,
            )
            archive_extract_all(tmp.file, toolchains_dir, progress_header="Extracting")
        with tempfile.NamedTemporaryFile() as tmp:
            _LOGGER.info("Downloading %s toolchain ...", _TOOLCHAIN_VERSION)
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
            archive_extract_all(
                tmp.file,
                toolchains_dir / "arm-zephyr-eabi",
                progress_header="Extracting",
            )
        sentinel.touch()
