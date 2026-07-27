import hashlib
import logging
import os
from pathlib import Path
import platform
import shutil
import sys
import tempfile

import platformdirs

import esphome.config_validation as cv
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
from esphome.helpers import get_str_env

_LOGGER = logging.getLogger(__name__)

_REQUIREMENTS = Path(__file__).parent / "requirements.txt"
TOOLCHAIN_VERSION = "0.17.4"

# Packages the PlatformIO toolchain's Zephyr build script needs beyond west
# (which comes from requirements.txt). Keep the pin in sync with
# framework-sdk-nrf scripts/platformio/platformio-build.py.
_PLATFORMIO_PENV_REQUIREMENTS: tuple[str, ...] = ("cbor2==5.6.5",)

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


def get_sdk_nrf_tools_path() -> Path:
    # A blank ESPHOME_SDK_NRF_PREFIX must be treated as unset: Path("")
    # resolves to the CWD, which clean-all would then delete.
    if prefix := get_str_env("ESPHOME_SDK_NRF_PREFIX", "").strip():
        path = Path(prefix).expanduser()
    else:
        # Machine-global (OS user cache dir) so all projects share one install;
        # see espidf.framework.get_idf_tools_path for the location rationale.
        path = Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "sdk-nrf"
    return path.resolve()


def _get_python_env_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "penvs" / version


def _get_framework_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "frameworks" / version


def _get_toolchain_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "toolchains" / version


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
    # ZEPHYR_SDK_INSTALL_DIR is the variable Zephyr documents for pointing at
    # the SDK: FindZephyr-sdk.cmake reads it (from the environment, via
    # zephyr_get) and passes it straight to find_package as a HINT. This
    # matters because the SDK lives in the esphome cache dir, which is not on
    # the module's static search path (/usr, /opt, $HOME, ...). A generic
    # "Zephyr-sdk_DIR" environment hint proved unreliable here: containerized
    # non-root builds failed to locate the SDK with it, while
    # ZEPHYR_SDK_INSTALL_DIR fixed the same invocation.
    env["ZEPHYR_SDK_INSTALL_DIR"] = str(_get_toolchain_path(TOOLCHAIN_VERSION))
    return env


def _get_platformio_penv_path() -> Path:
    return get_sdk_nrf_tools_path() / "penvs" / "platformio"


def _get_penv_site_packages(penv_path: Path) -> Path:
    if os.name == "nt":
        return penv_path / "Lib" / "site-packages"
    python_dir = f"python{sys.version_info.major}.{sys.version_info.minor}"
    return penv_path / "lib" / python_dir / "site-packages"


def _prepend_env_path(name: str, entry: str) -> None:
    """Prepend ``entry`` to the ``os.pathsep``-separated env var ``name``."""
    current = os.environ.get(name, "")
    entries = current.split(os.pathsep) if current else []
    if entry not in entries:
        os.environ[name] = os.pathsep.join([entry, *entries])


def setup_platformio_python_env() -> None:
    """Make the Zephyr build's Python packages available to PlatformIO.

    The PlatformIO toolchain's Zephyr framework build script pip-installs
    west and cbor2 (and pyocd on x86_64) into the Python environment running
    PlatformIO whenever they are not importable. That environment is not
    always writable — for example the docker image run as a non-root user,
    where ESPHome lives in the system Python — so the install fails with
    "Permission denied". Instead, pre-install those packages into a dedicated
    venv under the sdk-nrf tools dir and expose it to the PlatformIO
    subprocesses through the environment:

    * PYTHONPATH makes the venv's packages importable from the interpreter
      that runs PlatformIO/SCons, so the build script skips its installs.
    * VIRTUAL_ENV redirects any install the build script still performs via
      uv (pyocd is fetched on demand) into the writable venv.
    * PATH exposes console scripts installed into the venv (e.g. pyocd).
    """
    penv_path = _get_platformio_penv_path()
    env_python_path = get_python_env_executable_path(penv_path, "python")
    sentinel = penv_path / ".ready"
    # Include the Python version: the venv breaks when the interpreter it
    # was created from is upgraded, so it must be rebuilt.
    requirements_hash = hashlib.sha256(
        _REQUIREMENTS.read_bytes()
        + "\n".join(_PLATFORMIO_PENV_REQUIREMENTS).encode()
        + f"python{sys.version_info.major}.{sys.version_info.minor}".encode()
    ).hexdigest()
    if (
        not sentinel.exists()
        or sentinel.read_text(encoding="utf-8") != requirements_hash
    ):
        rmdir(penv_path, msg="Clean up PlatformIO toolchain Python environment")

        create_venv(penv_path, msg="PlatformIO toolchain")

        _LOGGER.info("Installing PlatformIO toolchain requirements ...")
        cmd = [
            str(env_python_path),
            "-m",
            "pip",
            "install",
            "-r",
            str(_REQUIREMENTS),
            *_PLATFORMIO_PENV_REQUIREMENTS,
        ]
        if not run_command_ok(cmd):
            raise EsphomeError(
                "Install requirements for PlatformIO toolchain Python environment failure"
            )
        sentinel.write_text(requirements_hash, encoding="utf-8")

    os.environ["VIRTUAL_ENV"] = str(penv_path)
    _prepend_env_path("PYTHONPATH", str(_get_penv_site_packages(penv_path)))
    _prepend_env_path("PATH", str(env_python_path.parent))


def _patch_uf2conv_escape_sequences(framework_path: Path) -> None:
    # SDK v2.6.1 ships uf2conv.py with '\s+' — an unrecognised escape that
    # Python 3.12+ flags with SyntaxWarning (a future version will reject it).
    uf2conv = framework_path / "zephyr" / "scripts" / "build" / "uf2conv.py"
    if not uf2conv.exists():
        return
    content = uf2conv.read_text(encoding="utf-8")
    patched = content.replace("re.split('\\s+', line)", "re.split('\\\\s+', line)")
    if patched == content:
        return
    # Write atomically so a concurrent build never sees a truncated file
    tmp = uf2conv.with_suffix(".py.tmp")
    tmp.write_text(patched, encoding="utf-8")
    shutil.copymode(uf2conv, tmp)
    tmp.replace(uf2conv)


def check_and_install() -> None:
    version = _get_version_str()
    python_env_path = _get_python_env_path(version)
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    sentinel = python_env_path / ".ready"
    requirements_hash = hashlib.sha256(_REQUIREMENTS.read_bytes()).hexdigest()
    install_venv = (
        not sentinel.exists()
        or sentinel.read_text(encoding="utf-8") != requirements_hash
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
        sentinel.write_text(requirements_hash, encoding="utf-8")

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
        framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
        if framework_ver < cv.Version(2, 9, 2):
            _patch_uf2conv_escape_sequences(framework_path)
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

    toolchains_dir = _get_toolchain_path(TOOLCHAIN_VERSION)
    sentinel = toolchains_dir / ".ready"
    if not sentinel.exists():
        rmdir(toolchains_dir, msg=f"Clean up {TOOLCHAIN_VERSION} toolchain environment")
        sysname, machine, extension = _get_toolchain_platform_info()
        with tempfile.NamedTemporaryFile() as tmp:
            _LOGGER.info("Downloading Zephyr SDK %s minimal ...", TOOLCHAIN_VERSION)
            download_from_mirrors(
                SDK_NG_MINIMAL_MIRRORS,
                {
                    "VERSION": TOOLCHAIN_VERSION,
                    "sysname": sysname,
                    "machine": machine,
                    "extension": extension,
                },
                tmp.file,
            )
            archive_extract_all(tmp.file, toolchains_dir, progress_header="Extracting")
        with tempfile.NamedTemporaryFile() as tmp:
            _LOGGER.info("Downloading %s toolchain ...", TOOLCHAIN_VERSION)
            download_from_mirrors(
                SDK_NG_TOOLCHAIN_MIRRORS,
                {
                    "VERSION": TOOLCHAIN_VERSION,
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
