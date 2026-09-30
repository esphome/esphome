from collections.abc import Iterator
import configparser
from contextlib import contextmanager
from dataclasses import dataclass, field
import hashlib
import logging
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

from esphome import yaml_util
from esphome.build_helpers.tools_cache import SDK_NRF_TOOLS_CACHE, tools_cache_path
from esphome.components.zephyr.const import KEY_SYSBUILD, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    create_venv,
    download_and_extract,
    get_python_env_executable_path,
    rmdir,
    run_command_ok,
    str_to_lst_of_str,
)

_LOGGER = logging.getLogger(__name__)

_REQUIREMENTS = Path(__file__).parent / "requirements.txt"
# NCS 3.4.0's bundled Zephyr (4.4.0) requires Zephyr SDK 1.0.1 (SDK_VERSION file in its
# zephyr/ tree) -- the SDK's own versioning scheme moved from 0.x to 1.x alongside this,
# and its release asset naming changed too (see SDK_NG_TOOLCHAIN_MIRRORS's "gnu_" infix).
TOOLCHAIN_VERSION = "1.0.1"

# sdk-nrf tag -> paired nrfconnect/ncs-zigbee tag. Nordic versions the two independently (no
# formula between them) -- add an entry here whenever RECOMMENDED_SDK_NRF_VERSION bumps and
# Nordic has shipped a paired ncs-zigbee release.
NCS_ZIGBEE_VERSIONS: dict[str, str] = {
    "v3.4.0": "v1.4.0",
}

# Packages the PlatformIO toolchain's Zephyr build script needs beyond west
# (which comes from requirements.txt). Keep the pin in sync with
# framework-sdk-nrf scripts/platformio/platformio-build.py.
_PLATFORMIO_PENV_REQUIREMENTS: tuple[str, ...] = ("cbor2==5.6.5",)

SDK_NG_TOOLCHAIN_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_TOOLCHAIN_MIRRORS",
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/toolchain_gnu_{sysname}-{machine}_arm-zephyr-eabi.{extension}",
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
    # Machine-global (OS user cache dir) so all projects share one install;
    # see espidf.framework.get_idf_tools_path for the location rationale.
    return tools_cache_path(*SDK_NRF_TOOLS_CACHE)


def _needs_venv_rebuild(
    env_python_path: Path, sentinel: Path, requirements_hash: str
) -> bool:
    """True when a penv must be (re)built.

    Rebuild when the interpreter is not a regular file, which covers a
    dangling symlink (a cached venv outliving a host interpreter upgrade)
    and a corrupt restore, or when the sentinel is missing or stale.
    """
    return (
        not env_python_path.is_file()
        or not sentinel.exists()
        or sentinel.read_text(encoding="utf-8") != requirements_hash
    )


def _get_python_env_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "penvs" / version


def _get_framework_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "frameworks" / version


def _get_toolchain_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "toolchains" / version


def toolchain_tool(name: str) -> Path:
    """Path to one of the pinned Zephyr SDK's tools (objdump, readelf, ...).

    The single owner of the ``arm-zephyr-eabi/bin/arm-zephyr-eabi-<name>``
    layout and the Windows suffix.
    """
    suffix = ".exe" if os.name == "nt" else ""
    bin_path = _get_toolchain_path(TOOLCHAIN_VERSION) / "arm-zephyr-eabi" / "bin"
    return bin_path / f"arm-zephyr-eabi-{name}{suffix}"


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


def _get_framework_cache_key(version: str, zigbee: bool) -> str:
    # A zigbee-enabled workspace has extra west projects (ncs-zigbee's ZBOSS
    # library) fetched into it that a plain one doesn't -- give it its own
    # cache dir rather than mutating an existing plain workspace in place.
    return f"{version}-zigbee" if zigbee else version


def get_build_paths(zigbee: bool = False) -> dict:
    version = _get_version_str()
    env_path = _get_python_env_path(version)
    return {
        "python_executable": get_python_env_executable_path(env_path, "python"),
        "codechecker_executable": get_python_env_executable_path(
            env_path, "CodeChecker"
        ),
        "framework_path": _get_framework_path(
            _get_framework_cache_key(version, zigbee)
        ),
    }


def get_build_env(zigbee: bool = False) -> dict:
    version = _get_version_str()
    venv_bin_dir = get_python_env_executable_path(
        _get_python_env_path(version), "python"
    ).parent
    env = os.environ.copy()
    env["PATH"] = str(venv_bin_dir) + os.pathsep + env.get("PATH", "")
    env["ZEPHYR_BASE"] = str(
        _get_framework_path(_get_framework_cache_key(version, zigbee)) / "zephyr"
    )
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
    if _needs_venv_rebuild(env_python_path, sentinel, requirements_hash):
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


def _generate_zigbee_manifest(framework_path: Path, version: str) -> Path:
    """Compose a local west manifest importing sdk-nrf and ncs-zigbee as sibling
    projects, so a single `west init -l` + `west update` fetches both together.

    ncs-zigbee is versioned independently of sdk-nrf (no formula between the two) --
    see NCS_ZIGBEE_VERSIONS. The root project must be named "nrf" (not the
    URL-basename "sdk-nrf") to match sdk-nrf's own manifest and NCS's sysbuild/
    Kconfig scripts, which hardcode that project name.
    """
    ncs_zigbee_version = NCS_ZIGBEE_VERSIONS.get(version)
    if ncs_zigbee_version is None:
        raise EsphomeError(
            f"zigbee: has no known compatible ncs-zigbee version for nRF Connect "
            f"SDK {version} -- add an entry to NCS_ZIGBEE_VERSIONS."
        )
    manifest_dir = framework_path / "esphome-manifest"
    manifest_yaml = yaml_util.dump(
        {
            "manifest": {
                "projects": [
                    {
                        "name": "nrf",
                        "url": "https://github.com/nrfconnect/sdk-nrf",
                        "revision": version,
                        "import": True,
                    },
                    {
                        "name": "ncs-zigbee",
                        "url": "https://github.com/nrfconnect/ncs-zigbee",
                        "revision": ncs_zigbee_version,
                        "import": True,
                    },
                ],
                "self": {"path": "esphome-manifest"},
            }
        }
    )
    manifest_dir.mkdir(parents=True, exist_ok=True)
    (manifest_dir / "west.yml").write_text(manifest_yaml, encoding="utf-8")

    # `west init -l` treats the given directory as a manifest repository -- its own
    # git working tree, not just a plain directory of files.
    if not run_command_ok(["git", "init", "-q"], cwd=str(manifest_dir)):
        raise EsphomeError("Can't initialize the generated Zephyr manifest repository")
    if not run_command_ok(["git", "add", "west.yml"], cwd=str(manifest_dir)):
        raise EsphomeError("Can't stage the generated Zephyr manifest")
    # -c user.*: this commit is purely internal (never pushed, never read by a
    # human), so it shouldn't depend on the user's own global git identity.
    run_command_ok(
        [
            "git",
            "-c",
            "user.name=ESPHome",
            "-c",
            "user.email=esphome@esphome.io",
            "commit",
            "-q",
            "--allow-empty",
            "-m",
            "esphome-generated Zephyr manifest",
        ],
        cwd=str(manifest_dir),
    )
    return manifest_dir


# hidapi builds two Linux C extensions: "hid" (libusb backend) and "hidraw"
# (udev backend) -- each needs its own -dev package.
_HIDAPI_APT_PACKAGES = ("pkg-config", "libusb-1.0-0-dev", "libudev-dev")
_HIDAPI_PKG_CONFIG_MODULES = ("libusb-1.0", "libudev")


def _ensure_hidapi_build_deps() -> None:
    """Install pkg-config/libusb-1.0-0-dev/libudev-dev if hidapi's wheel build needs them.

    Remove when these are added to ghcr.io/esphome/docker-base:debian-* --
    hidapi's wheel build (pulled in for west/pyOCD tooling) needs all three,
    and none is a pip package.
    """
    have_pkg_config = shutil.which("pkg-config") is not None
    have_deps = have_pkg_config and all(
        subprocess.run(["pkg-config", "--exists", module], check=False).returncode == 0
        for module in _HIDAPI_PKG_CONFIG_MODULES
    )
    if have_deps or shutil.which("apt") is None:
        return
    if os.geteuid() != 0:
        raise EsphomeError(
            "pkg-config, libusb-1.0-0-dev, and libudev-dev are required to "
            "build Zephyr requirements. Install them with: sudo apt install "
            + " ".join(_HIDAPI_APT_PACKAGES)
        )
    _LOGGER.info("Installing %s ...", ", ".join(_HIDAPI_APT_PACKAGES))
    if not run_command_ok(["apt", "update"]):
        raise EsphomeError("Failed to update apt package index")
    if not run_command_ok(["apt", "install", "-y", *_HIDAPI_APT_PACKAGES]):
        raise EsphomeError(f"Failed to install {'/'.join(_HIDAPI_APT_PACKAGES)}")


# West projects every build needs; components add others with include_west_project()
DEFAULT_WEST_PROJECTS = ("cmsis", "hal_nordic", "nrfxlib", "zephyr")

_KEY_NRF52 = "nrf52"
# The projects a finished install fetched
_WEST_PROJECTS_FILE = ".west_projects"


@dataclass
class _Nrf52Data:
    west_projects: set[str] = field(default_factory=lambda: set(DEFAULT_WEST_PROJECTS))


def _get_data() -> _Nrf52Data:
    if _KEY_NRF52 not in CORE.data:
        CORE.data[_KEY_NRF52] = _Nrf52Data()
    return CORE.data[_KEY_NRF52]


def include_west_project(name: str) -> None:
    """Fetch a west project left out by default; call from to_code()."""
    _get_data().west_projects.add(name)


def bluetooth_west_projects() -> tuple[str, ...]:
    """Bluetooth's crypto: TinyCrypt up to SDK 3.1, PSA (mbedtls, Oberon) from 3.2."""
    if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(3, 2, 0):
        return ("mbedtls", "oberon-psa-crypto")
    return ("tinycrypt",)


def openthread_west_projects() -> tuple[str, ...]:
    """Crypto for OpenThread: mbedtls, plus Oberon from SDK 2.7."""
    if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(2, 7, 0):
        return ("mbedtls", "openthread", "oberon-psa-crypto")
    return ("mbedtls", "openthread")


def _wanted_west_projects() -> set[str]:
    projects = set(_get_data().west_projects)
    # Zephyr 4.1 moved the Cortex-M core headers to cmsis_6
    if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(3, 1, 0):
        projects.add("cmsis_6")
    # Sysbuild builds the MCUboot image with any bootloader
    if CORE.data.get(KEY_ZEPHYR, {}).get(KEY_SYSBUILD):
        projects.add("mcuboot")
    return projects


def _set_project_filter(
    env_python_path: Path, framework_path: Path, projects: set[str]
) -> bool:
    # "--" keeps west from reading the leading "-" as an option
    project_filter = ",".join(["-.*", *(f"+{p}" for p in sorted(projects))])
    cmd = [str(env_python_path), "-m", "west", "config", "manifest.project-filter"]
    return run_command_ok([*cmd, "--", project_filter], cwd=framework_path)


def _check_west_projects(
    env_python_path: Path, framework_path: Path, version: str, projects: set[str]
) -> None:
    """Raise when the manifest lacks one of ``projects``; needs zephyr cloned.

    west update quietly skips an unknown name in the filter, west list fails on it.
    """
    names = sorted(projects)
    cmd = [str(env_python_path), "-m", "west", "list", "-f", "{name}", *names]
    if not run_command_ok(cmd, cwd=framework_path):
        raise EsphomeError(
            f"west list failed for the requested nRF Connect SDK {version} projects "
            f"({', '.join(names)}); a project the manifest does not have is the "
            "usual cause, see west's output above"
        )


def _west_update(
    env_python_path: Path,
    framework_path: Path,
    version: str,
    projects: set[str] | None,
    checked: bool = False,
) -> bool:
    """Fetch ``projects``, or every project when None; False when the fetch fails."""
    if projects is not None and not _set_project_filter(
        env_python_path, framework_path, projects
    ):
        return False
    cmd = [
        str(env_python_path),
        "-m",
        "west",
        "update",
        "--narrow",
        "--fetch-opt=--depth=1",
    ]
    # Streamed so the clone's progress reaches the log
    if not run_command_ok(cmd, cwd=framework_path, stream_output=True):
        return False
    if projects is None:
        return True
    if not checked:
        _check_west_projects(env_python_path, framework_path, version, projects)
    (framework_path / _WEST_PROJECTS_FILE).write_text(
        "\n".join(sorted(projects)), encoding="utf-8"
    )
    return True


def _installed_west_projects(framework_path: Path) -> set[str] | None:
    """The projects a finished install fetched; None when it has every project."""
    try:
        stamp = (framework_path / _WEST_PROJECTS_FILE).read_text(encoding="utf-8")
    except FileNotFoundError:
        pass
    else:
        return set(stamp.split())
    # No stamp: an install from before the filter has every project, a filtered
    # one that lost its stamp fetches again
    config = configparser.ConfigParser()
    if not config.read(framework_path / ".west" / "config", encoding="utf-8"):
        return set()
    if config.has_option("manifest", "project-filter"):
        return set()
    return None


def _restore_project_filter(
    env_python_path: Path, framework_path: Path, version: str, installed: set[str]
) -> None:
    """Put the filter back to the stamp's projects; the defaults always stay in."""
    projects = installed | set(DEFAULT_WEST_PROJECTS)
    if not _set_project_filter(env_python_path, framework_path, projects):
        _LOGGER.warning(
            "Couldn't put the nRF Connect SDK %s project filter back; "
            "the next build that fetches a project sets it again",
            version,
        )


# Lock wait slices, so Ctrl-C stays responsive
_INSTALL_LOCK_POLL = 1


@contextmanager
def _install_lock(name: str) -> Iterator[None]:
    """Serialize a shared install step across builds running at once."""
    from filelock import FileLock, Timeout

    lock_path = get_sdk_nrf_tools_path() / f"{name}.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    # No soft lock: its marker outlives a killed build and hangs every later one
    lock = FileLock(str(lock_path), fallback_to_soft=False)
    waiting = False
    while True:
        try:
            lock.acquire(timeout=_INSTALL_LOCK_POLL)
            break
        except Timeout:  # before OSError, which it subclasses
            if not waiting:
                waiting = True
                _LOGGER.info("Waiting for another build installing %s ...", name)
        except OSError as err:
            _LOGGER.warning(
                "Can't lock %s (%s), continuing without a lock", lock_path, err
            )
            break
    try:
        yield
    finally:
        lock.release()


def _fetch_missing_west_projects(
    env_python_path: Path, framework_path: Path, version: str, projects: set[str]
) -> None:
    """Fetch the wanted projects a finished install lacks; it only ever gains."""
    if projects <= set(DEFAULT_WEST_PROJECTS):
        return
    installed = _installed_west_projects(framework_path)
    # Before the fetch, so an unknown name costs nothing on any install
    _check_west_projects(env_python_path, framework_path, version, projects)
    if installed is None or not (missing := projects - installed):
        return
    _LOGGER.info(
        "Fetching nRF Connect SDK %s projects: %s", version, ", ".join(sorted(missing))
    )
    wanted = installed | projects
    if not _west_update(env_python_path, framework_path, version, wanted, checked=True):
        _restore_project_filter(env_python_path, framework_path, version, installed)
        raise EsphomeError(f"Can't update nRF Connect SDK {version}")


def _install_framework(
    env_python_path: Path,
    framework_path: Path,
    version: str,
    projects: set[str] | None,
    zigbee: bool = False,
) -> None:
    """Clone the nRF Connect SDK into ``framework_path`` with west.

    ``projects`` None fetches every project. ``zigbee`` also clones ncs-zigbee.

    A download cut short after ``west init`` leaves the workspace behind;
    rerunning ``west update`` there only fetches what is missing, so it resumes
    instead of cloning about 2 GB again. A resume that fails keeps what was
    fetched (a flaky network is the likely cause) and is retried on the next
    build; only a second failure in a row starts over clean.
    """
    resume_failed = framework_path / ".resume_failed"
    # Resume only a workspace whose ``west init`` finished (it writes the
    # config last). ``.ready`` with missing requirements is a damaged install,
    # not an interrupted one, so it goes the clean way.
    initialized = (framework_path / ".west" / "config").is_file()
    if initialized and not (framework_path / ".ready").exists():
        _LOGGER.info("Resuming the nRF Connect SDK %s download ...", version)
        if _west_update(env_python_path, framework_path, version, projects):
            resume_failed.unlink(missing_ok=True)
            return
        if not resume_failed.exists():
            resume_failed.touch()
            raise EsphomeError(
                f"Can't resume the nRF Connect SDK {version} download; "
                "the next build retries it"
            )
        _LOGGER.warning(
            "Resuming failed again; downloading nRF Connect SDK %s anew", version
        )
    rmdir(framework_path, msg=f"Clean up {version} framework environment")
    _LOGGER.info("Initializing nRF Connect SDK %s ...", version)
    if zigbee:
        manifest_dir = _generate_zigbee_manifest(framework_path, version)
        cmd = [str(env_python_path), "-m", "west", "init", "-l", str(manifest_dir)]
    else:
        cmd = [
            str(env_python_path),
            "-m",
            "west",
            "init",
            "-m",
            "https://github.com/nrfconnect/sdk-nrf",
            "-o=--depth=1",
            "--mr",
            version,
            str(framework_path),
        ]
    if not run_command_ok(cmd, stream_output=True):
        raise EsphomeError(f"Can't initialize nRF Connect SDK {version}")
    _LOGGER.info("Updating nRF Connect SDK %s (this may take a while) ...", version)
    if not _west_update(env_python_path, framework_path, version, projects):
        raise EsphomeError(f"Can't update nRF Connect SDK {version}")


def check_and_install(zigbee: bool = False) -> None:
    version = _get_version_str()
    with _install_lock(f"sdk-{version}"):
        _check_and_install(version, zigbee)


def _check_and_install(version: str, zigbee: bool) -> None:
    python_env_path = _get_python_env_path(version)
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    sentinel = python_env_path / ".ready"
    requirements_hash = hashlib.sha256(_REQUIREMENTS.read_bytes()).hexdigest()
    install_venv = _needs_venv_rebuild(env_python_path, sentinel, requirements_hash)
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

    framework_path = _get_framework_path(_get_framework_cache_key(version, zigbee))
    sentinel = framework_path / ".ready"
    zephyr_reqs = framework_path / "zephyr" / "scripts" / "requirements.txt"
    projects = _wanted_west_projects()
    if not sentinel.exists() or not zephyr_reqs.exists():
        _install_framework(
            env_python_path,
            framework_path,
            version,
            # A zigbee workspace fetches every project, as before the filter
            None if zigbee else projects,
            zigbee,
        )
        framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
        if framework_ver < cv.Version(2, 9, 2):
            _patch_uf2conv_escape_sequences(framework_path)
        sentinel.touch()
    else:
        _fetch_missing_west_projects(env_python_path, framework_path, version, projects)

    zephyr_sentinel = python_env_path / ".zephyr_reqs_ready"
    if (
        install_venv
        or not zephyr_sentinel.exists()
        or zephyr_reqs.stat().st_mtime > zephyr_sentinel.stat().st_mtime
    ):
        _ensure_hidapi_build_deps()

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

    # Shared by every SDK version; locked only while missing
    if not (_get_toolchain_path(TOOLCHAIN_VERSION) / ".ready").exists():
        with _install_lock(f"toolchain-{TOOLCHAIN_VERSION}"):
            _install_toolchain()


def _install_toolchain() -> None:
    toolchains_dir = _get_toolchain_path(TOOLCHAIN_VERSION)
    sentinel = toolchains_dir / ".ready"
    if not sentinel.exists():
        rmdir(toolchains_dir, msg=f"Clean up {TOOLCHAIN_VERSION} toolchain environment")
        sysname, machine, extension = _get_toolchain_platform_info()
        substitutions = {
            "VERSION": TOOLCHAIN_VERSION,
            "sysname": sysname,
            "machine": machine,
            "extension": extension,
        }
        # Downloaded next to the destination (not a temp file) so an
        # interrupted download's .part file resumes on the next run.
        for mirrors, extract_dir, what, slug in (
            (SDK_NG_MINIMAL_MIRRORS, toolchains_dir, "Zephyr SDK minimal", "minimal"),
            (
                SDK_NG_TOOLCHAIN_MIRRORS,
                # SDK 1.0.1's CMake scripts (cmake/zephyr/gnu/generic.cmake) glob for
                # the cross-compiler under $ZEPHYR_SDK_INSTALL_DIR/gnu/*-*zephyr-*,
                # not flat at the toolchain root as the pre-1.0 SDK generation
                # expected.
                toolchains_dir / "gnu" / "arm-zephyr-eabi",
                "toolchain",
                "toolchain",
            ),
        ):
            _LOGGER.info("Downloading %s %s ...", TOOLCHAIN_VERSION, what)
            download_and_extract(
                mirrors,
                substitutions,
                toolchains_dir.with_name(f"{toolchains_dir.name}.{slug}.archive"),
                extract_dir,
                progress_header="Extracting",
            )
        # Best-effort prune of resume leftovers, including a previous
        # TOOLCHAIN_VERSION's orphans; the SDK archives are hundreds of MB.
        # A locked file must not discard the just-completed install.
        for leftover in toolchains_dir.parent.glob("*.archive.part*"):
            try:
                leftover.unlink()
            except OSError as err:
                _LOGGER.debug("Could not remove %s: %s", leftover, err)
        sentinel.touch()
