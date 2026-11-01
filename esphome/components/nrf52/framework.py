from collections.abc import Iterator
import configparser
from contextlib import contextmanager
from dataclasses import dataclass, field
import hashlib
import json
import logging
import os
from pathlib import Path
import platform
import shutil
import sys

from esphome.build_helpers.ccache import ccache_env
from esphome.build_helpers.tools_cache import SDK_NRF_TOOLS_CACHE, tools_cache_path
from esphome.components.zephyr.const import KEY_SYSBUILD, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    create_venv,
    download_and_extract,
    download_with_resume,
    get_python_env_executable_path,
    rmdir,
    run_command_ok,
    str_to_lst_of_str,
)

_LOGGER = logging.getLogger(__name__)

_REQUIREMENTS = Path(__file__).parent / "requirements.txt"
# Run with the SDK's Python at installation, for SDK versions boards.py has no data for
_GENERATE_BOARDS = Path(__file__).parent / "generate_boards.py"
_BOARD_DATA_FILE = "esphome_boards.json"
TOOLCHAIN_VERSION = "0.17.4"
# Zephyr SDK used by nRF Connect SDK 3.4.0 and newer.
_TOOLCHAIN_VERSION_NCS_3_4_0 = "1.0.1"
_TOOLCHAIN_VERSIONS = (TOOLCHAIN_VERSION, _TOOLCHAIN_VERSION_NCS_3_4_0)


def _uses_sdk_ng_1_toolchain() -> bool:
    """True when the framework needs Zephyr SDK 1.0+.

    SDK 1.0 moved the GNU toolchain under gnu/ and renamed its archive
    to toolchain_gnu_*.
    """
    return CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(3, 4, 0)


def _get_toolchain_version() -> str:
    """Return the Zephyr SDK toolchain version for the current framework."""
    if _uses_sdk_ng_1_toolchain():
        return _TOOLCHAIN_VERSION_NCS_3_4_0
    return TOOLCHAIN_VERSION


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
_SDK_NG_TOOLCHAIN_GNU_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_TOOLCHAIN_GNU_MIRRORS",
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/toolchain_gnu_{sysname}-{machine}_arm-zephyr-eabi.{extension}",
    )
)


def _get_sdk_ng_toolchain_mirrors() -> list[str]:
    """Return toolchain mirror URLs for the current framework version."""
    if _uses_sdk_ng_1_toolchain():
        return _SDK_NG_TOOLCHAIN_GNU_MIRRORS
    return SDK_NG_TOOLCHAIN_MIRRORS


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


def _get_arm_toolchain_path() -> Path:
    """The arm-zephyr-eabi directory inside the pinned Zephyr SDK.

    The single owner of the SDK 0.x (arm-zephyr-eabi/) and SDK 1.0+
    (gnu/arm-zephyr-eabi/) layouts.
    """
    toolchain_root = _get_toolchain_path(_get_toolchain_version())
    if _uses_sdk_ng_1_toolchain():
        return toolchain_root / "gnu" / "arm-zephyr-eabi"
    return toolchain_root / "arm-zephyr-eabi"


def toolchain_tool(name: str) -> Path:
    """Path to one of the pinned Zephyr SDK's tools (objdump, readelf, ...)."""
    suffix = ".exe" if os.name == "nt" else ""
    return _get_arm_toolchain_path() / "bin" / f"arm-zephyr-eabi-{name}{suffix}"


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


def get_build_env(ccache: str | None) -> dict:
    """Build the west/sdk-nrf process environment.

    ``ccache`` is the resolved binary (resolve_ccache_path), or None when
    ccache is disabled or the caller never compiles; it brings the shared
    managed-ccache settings and the pch sloppiness, so every caller that
    may compile gets the same cache.
    """
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
    env["ZEPHYR_SDK_INSTALL_DIR"] = str(_get_toolchain_path(_get_toolchain_version()))
    if ccache is None:
        # Zephyr wraps compiles with any ccache it finds; unmanaged it
        # must not cache (a sysbuild image never sees USE_CCACHE=0).
        env.setdefault("CCACHE_DISABLE", "1")
    else:
        env.update(ccache_env(ccache, SDK_NRF_TOOLS_CACHE))
        # Drop only the per build map entry (posix, CMake's spelling);
        # its from side covers no compiled sources. A spaced path cannot
        # survive ccache's space split list, so it stays hashed.
        source_dir = CORE.relative_build_path("zephyr").as_posix()
        if any(ch.isspace() for ch in source_dir):
            _LOGGER.debug(
                "Whitespace in %s; the per build map stays hashed", source_dir
            )
        else:
            device_map = f"-fmacro-prefix-map={source_dir}=CMAKE_SOURCE_DIR"
            env["CCACHE_IGNOREOPTIONS"] = (
                f"{env.get('CCACHE_IGNOREOPTIONS', '')} {device_map}".strip()
            )
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


def _patch_framework_file(path: Path, old: str, new: str) -> bool:
    """Replace ``old`` with ``new`` in a framework script, atomically and
    keeping the file mode (helpers.write_file would flatten it to 0o644).
    Returns False when nothing matched."""
    import tempfile

    content = path.read_text(encoding="utf-8")
    patched = content.replace(old, new)
    if patched == content:
        return False
    # Unique sibling tmp: the install lock is best effort, so two builds
    # may patch at once and a shared tmp name could rename a half
    # written file into place.
    fd, tmp_name = tempfile.mkstemp(dir=path.parent, suffix=".tmp")
    tmp = Path(tmp_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(patched)
        shutil.copymode(path, tmp)
        tmp.replace(path)
    except BaseException:
        tmp.unlink(missing_ok=True)
        raise
    return True


def _patch_uf2conv_escape_sequences(framework_path: Path) -> None:
    # SDK v2.6.1 ships uf2conv.py with '\s+' — an unrecognised escape that
    # Python 3.12+ flags with SyntaxWarning (a future version will reject it).
    uf2conv = framework_path / "zephyr" / "scripts" / "build" / "uf2conv.py"
    if uf2conv.exists():
        _patch_framework_file(
            uf2conv, "re.split('\\s+', line)", "re.split('\\\\s+', line)"
        )


_WEST_BUILD_RELPATH = "        srcrel = os.path.relpath(self.source_dir)\n"
# Zephyr main's replacement, verbatim (zephyr commit dfbdca0e13)
_WEST_BUILD_RELPATH_SAFE = """        try:
            srcrel = pathlib.Path(self.source_dir).relative_to(pathlib.Path.cwd(), walk_up=True)
        except ValueError:
            srcrel = self.source_dir
"""


def _patch_west_build_source_dir_relpath(framework_path: Path) -> None:
    # west build names the source dir relative to the working directory (the
    # SDK workspace) in two warnings; Python refuses that on Windows when the
    # project is on another drive, and west dies before building. Applies
    # Zephyr main's fix; SDKs that carry it have no line to patch.
    build = framework_path / "zephyr" / "scripts" / "west_commands" / "build.py"
    if build.exists():
        _patch_framework_file(build, _WEST_BUILD_RELPATH, _WEST_BUILD_RELPATH_SAFE)


def _patch_gen_defines_dts_path(framework_path: Path) -> None:
    # The absolute zephyr.dts.pre path in the header's top comment is
    # its only per device byte and blocks ccache sharing; emit the
    # basename. Upstream candidate.
    gen_defines = framework_path / "zephyr" / "scripts" / "dts" / "gen_defines.py"
    if not gen_defines.exists():
        return
    if _patch_framework_file(
        gen_defines, "  {edt.dts_path}", "  {os.path.basename(edt.dts_path)}"
    ):
        return
    if "{os.path.basename(edt.dts_path)}" not in gen_defines.read_text(
        encoding="utf-8"
    ):
        # Upstream reformatted the comment; sharing silently degrading
        # would be invisible, so say it out loud.
        _LOGGER.warning(
            "gen_defines.py no longer matches; the devicetree header "
            "stays per device and ccache sharing between devices degrades"
        )


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


def wanted_west_projects() -> set[str]:
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
    projects: set[str],
    checked: bool = False,
) -> bool:
    """Fetch ``projects``; False when the fetch fails."""
    if not _set_project_filter(env_python_path, framework_path, projects):
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
    env_python_path: Path, framework_path: Path, version: str, projects: set[str]
) -> None:
    """Clone the nRF Connect SDK into ``framework_path`` with west.

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


def check_and_install() -> None:
    version = _get_version_str()
    with _install_lock(f"sdk-{version}"):
        _check_and_install(version)


def _check_and_install(version: str) -> None:
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

    framework_path = _get_framework_path(version)
    sentinel = framework_path / ".ready"
    zephyr_reqs = framework_path / "zephyr" / "scripts" / "requirements.txt"
    projects = wanted_west_projects()
    if not sentinel.exists() or not zephyr_reqs.exists():
        _install_framework(env_python_path, framework_path, version, projects)
        framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
        if framework_ver < cv.Version(2, 9, 2):
            _patch_uf2conv_escape_sequences(framework_path)
        sentinel.touch()
    else:
        _fetch_missing_west_projects(env_python_path, framework_path, version, projects)
    # Every run: existing installs need these too, and they are no-ops once applied
    _patch_gen_defines_dts_path(framework_path)
    _patch_west_build_source_dir_relpath(framework_path)

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

    # Shared by every SDK version that uses the same toolchain; locked only
    # while missing
    toolchain_version = _get_toolchain_version()
    if not (_get_toolchain_path(toolchain_version) / ".ready").exists():
        with _install_lock(f"toolchain-{toolchain_version}"):
            _install_toolchain()

    # Needs the toolchain's preprocessor, so it comes last
    _generate_board_data(env_python_path, framework_path, version)


def _generator_hash() -> str:
    return hashlib.sha256(_GENERATE_BOARDS.read_bytes()).hexdigest()


def load_installed_board_data(framework_version: cv.Version) -> dict[str, dict] | None:
    """Return the board data read from the installed SDK of framework_version.

    None when that SDK is not installed yet, the data could not be read, or it was read by an
    older generate_boards.py.
    """
    version = f"v{framework_version.major}.{framework_version.minor}.{framework_version.patch}"
    path = _get_framework_path(version) / _BOARD_DATA_FILE
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict) or data.get("generator") != _generator_hash():
        return None
    return data.get("boards")


def _generate_board_data(
    env_python_path: Path, framework_path: Path, version: str
) -> None:
    """Read the board device trees of the installed SDK into _BOARD_DATA_FILE."""
    if (
        load_installed_board_data(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
        is not None
    ):
        return
    _LOGGER.info("Reading the board device trees of nRF Connect SDK %s ...", version)
    cmd = [
        str(env_python_path),
        str(_GENERATE_BOARDS),
        "--cpp",
        str(toolchain_tool("cpp")),
        "--generator",
        _generator_hash(),
        "--json",
        str(framework_path / _BOARD_DATA_FILE),
        str(framework_path / "zephyr"),
        str(framework_path / "nrf"),
    ]
    devicetree = framework_path / "zephyr" / "scripts" / "dts" / "python-devicetree"
    if not run_command_ok(cmd, env={"PYTHONPATH": str(devicetree / "src")}):
        _LOGGER.warning(
            "Can't read the board device trees of nRF Connect SDK %s; board-specific "
            "settings use the data of the default SDK version",
            version,
        )


# Nordic's nrfutil launcher, one static binary per host; its commands are installed from Nordic's
# package index by the launcher itself. The path carries no version, so the file is pinned by the
# size and digest Nordic's file store reported for launcher 8.2.1 (2026-09-09)
NORDIC_NRFUTIL_URL = "https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/{triple}/{name}"
_NRFUTIL_LAUNCHERS: dict[str, tuple[int, str]] = {
    "x86_64-unknown-linux-gnu": (
        6725408,
        "1cf4bd1d3d64f43a6b9602d8d8e8d647bec0d26d517ba72fd154f95546b50260",
    ),
    "aarch64-unknown-linux-gnu": (
        5506040,
        "e7310f072132766e69b43c833c6a87a53367ea68d5b8a5f7c6fa1a45993ecde4",
    ),
    "aarch64-apple-darwin": (
        5263376,
        "1b993eece5a6e3e079ce25fa31a134ce4c12fb51735d10114a2ea4a1df048591",
    ),
    "x86_64-apple-darwin": (
        6389488,
        "a5ffea65325e5d38eae00f612472e18ffe2c036f800240873d9f702c1b53d945",
    ),
    "x86_64-pc-windows-msvc": (
        5939032,
        "22cb1bd03fc29016670c1fc8408a78bd213286ecc1156c70712485d77b097c75",
    ),
}
_NRFUTIL_OS_SUFFIX = {
    "linux": "unknown-linux-gnu",
    "macos": "apple-darwin",
    "windows": "pc-windows-msvc",
}
# The nrf5sdk-tools command gives the launcher the pkg and dfu subcommands; a new version reinstalls
NRF5SDK_TOOLS_VERSION = "1.1.0"


def _nrfutil_target_triple() -> str:
    sysname, machine, _ = _get_toolchain_platform_info()
    triple = f"{machine}-{_NRFUTIL_OS_SUFFIX.get(sysname, sysname)}"
    if triple not in _NRFUTIL_LAUNCHERS:
        raise EsphomeError(f"Nordic's nrfutil is not available for {sysname} {machine}")
    return triple


def ensure_nordic_nrfutil(env: dict[str, str]) -> str:
    """Nordic's nrfutil with its nrf5sdk-tools command, downloaded into the tools cache on first use.

    The launcher keeps its installed commands under NRFUTIL_HOME, which is set in ``env`` so that
    the copy here never touches a user's own ~/.nrfutil. Raises EsphomeError when the download or
    the command install fails, for instance without network.
    """
    # The downloaded file is a bootstrap launcher: on its first run it installs the real core into
    # NRFUTIL_HOME/bin and runs that, so it must not sit there itself or it runs itself forever
    root = get_sdk_nrf_tools_path() / "nrfutil"
    home = root / "home"
    suffix = ".exe" if os.name == "nt" else ""
    exe = root / "launcher" / f"nrfutil{suffix}"
    command = home / "bin" / f"nrfutil-nrf5sdk-tools{suffix}"
    env["NRFUTIL_HOME"] = str(home)
    sentinel = root / f".ready-{NRF5SDK_TOOLS_VERSION}"

    # A cache that lost its launcher or command (a partial clean-up) is installed again
    def installed() -> bool:
        return sentinel.exists() and exe.is_file() and command.is_file()

    if installed():
        return str(exe)
    with _install_lock("nrfutil"):
        if not installed():
            exe.parent.mkdir(parents=True, exist_ok=True)
            _LOGGER.info("Downloading Nordic's nrfutil ...")
            triple = _nrfutil_target_triple()
            size, sha256 = _NRFUTIL_LAUNCHERS[triple]
            download_with_resume(
                NORDIC_NRFUTIL_URL.format(triple=triple, name=exe.name),
                exe,
                sha256=sha256,
                size=size,
            )
            exe.chmod(0o755)
            _LOGGER.info("Installing nrfutil's nrf5sdk-tools command ...")
            if (
                not run_command_ok(
                    [str(exe), "install", f"nrf5sdk-tools={NRF5SDK_TOOLS_VERSION}"],
                    env=env,
                )
                or not command.is_file()
            ):
                raise EsphomeError("Failed to install nrfutil's nrf5sdk-tools command")
            sentinel.touch()
    return str(exe)


def _install_toolchain() -> None:
    toolchain_version = _get_toolchain_version()
    toolchains_dir = _get_toolchain_path(toolchain_version)
    sentinel = toolchains_dir / ".ready"
    if not sentinel.exists():
        rmdir(toolchains_dir, msg=f"Clean up {toolchain_version} toolchain environment")
        sysname, machine, extension = _get_toolchain_platform_info()
        substitutions = {
            "VERSION": toolchain_version,
            "sysname": sysname,
            "machine": machine,
            "extension": extension,
        }
        # Downloaded next to the destination (not a temp file) so an
        # interrupted download's .part file resumes on the next run.
        # SDK 1.0+ Zephyr-sdkConfig.cmake looks for the toolchain in
        # gnu/arm-zephyr-eabi/; extraction strips the archive's single root.
        for mirrors, extract_dir, what, slug in (
            (SDK_NG_MINIMAL_MIRRORS, toolchains_dir, "Zephyr SDK minimal", "minimal"),
            (
                _get_sdk_ng_toolchain_mirrors(),
                _get_arm_toolchain_path(),
                "toolchain",
                "toolchain",
            ),
        ):
            _LOGGER.info("Downloading %s %s ...", toolchain_version, what)
            download_and_extract(
                mirrors,
                substitutions,
                toolchains_dir.with_name(f"{toolchains_dir.name}.{slug}.archive"),
                extract_dir,
                progress_header="Extracting",
            )
        # Best-effort prune of resume leftovers, including orphans of retired
        # toolchain versions; the SDK archives are hundreds of MB. The other
        # toolchain still in use may be downloading under its own lock, so its
        # leftovers are kept. A locked file must not discard the just-completed
        # install.
        other_versions = tuple(
            f"{v}." for v in _TOOLCHAIN_VERSIONS if v != toolchain_version
        )
        for leftover in toolchains_dir.parent.glob("*.archive.part*"):
            if leftover.name.startswith(other_versions):
                continue
            try:
                leftover.unlink()
            except OSError as err:
                _LOGGER.debug("Could not remove %s: %s", leftover, err)
        sentinel.touch()
