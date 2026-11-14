import configparser
from dataclasses import dataclass, field
import hashlib
import logging
import os
from pathlib import Path
import re
import sys

from esphome.build_helpers.ccache import ccache_env
from esphome.build_helpers.tools_cache import SDK_NRF_TOOLS_CACHE, tools_cache_path
from esphome.components.zephyr.const import KEY_SYSBUILD, KEY_ZEPHYR
from esphome.components.zephyr.framework_helpers import (
    ensure_venv,
    needs_venv_rebuild,
    patch_framework_file,
    patch_gen_defines_dts_path,
    patch_west_build_source_dir_relpath,
)
from esphome.components.zephyr.sdk_install import (
    ARM_TOOLCHAIN,
    get_sdk_host_info,
    install_sdk,
    sdk_toolchain_path,
)
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    create_venv,
    download_with_resume,
    get_python_env_executable_path,
    install_lock,
    rmdir,
    run_command_ok,
)

from .const import VARIANT_NRF52840

_LOGGER = logging.getLogger(__name__)

_REQUIREMENTS = Path(__file__).parent / "requirements.txt"
TOOLCHAIN_VERSION = "0.17.4"
# Zephyr SDK used by nRF Connect SDK 3.4.0 and newer.
_TOOLCHAIN_VERSION_NCS_3_4_0 = "1.0.1"
_TOOLCHAIN_VERSIONS = (TOOLCHAIN_VERSION, _TOOLCHAIN_VERSION_NCS_3_4_0)


def _uses_sdk_ng_1_toolchain() -> bool:
    """True when the framework needs Zephyr SDK 1.0+."""
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


def get_sdk_nrf_tools_path() -> Path:
    # Machine-global (OS user cache dir) so all projects share one install;
    # see espidf.framework.get_idf_tools_path for the location rationale.
    return tools_cache_path(*SDK_NRF_TOOLS_CACHE)


def _get_python_env_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "penvs" / version


def _get_framework_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "frameworks" / version


def _get_toolchain_path(version: str) -> Path:
    return get_sdk_nrf_tools_path() / "toolchains" / version


def _get_arm_toolchain_path() -> Path:
    """The arm-zephyr-eabi directory inside the pinned Zephyr SDK."""
    toolchain_version = _get_toolchain_version()
    return sdk_toolchain_path(
        _get_toolchain_path(toolchain_version), toolchain_version, ARM_TOOLCHAIN
    )


def toolchain_tool(name: str) -> Path:
    """Path to one of the pinned Zephyr SDK's tools (objdump, readelf, ...)."""
    suffix = ".exe" if os.name == "nt" else ""
    return _get_arm_toolchain_path() / "bin" / f"arm-zephyr-eabi-{name}{suffix}"


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
    if needs_venv_rebuild(env_python_path, sentinel, requirements_hash):
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


# pyOCD 0.44 to 0.46 write the nRF54LM20A RRAM without its write buffer, which faults on the first
# store (pyocd/pyOCD#2016), and the Seeed XIAO nRF54LM20A boards flash through pyOCD. This is the
# flash algorithm of pyocd/pyOCD#2033, which fixes it, put in place until a release carries it.
_PYOCD_NRF54LM20A_FLASH_ALGO = """\
FLASH_ALGO = {
    'load_address' : 0x20000000,

    # Flash algorithm as a hex string
    'instructions': [
        0xe7fdbe00,
        0xf8d24a02, 0x07db3400, 0x4770d5fb, 0x5004e000, 0xf7ffb508, 0x2000fff5, 0x2000bd08, 0x00004770,
        0x49072001, 0xf8c1b508, 0xf7ff0500, 0xf8c1ffe9, 0x20000540, 0xffe4f7ff, 0x0500f8c1, 0xbf00bd08,
        0x5004e000, 0xf242b508, 0x49080301, 0x3500f8c1, 0xffd6f7ff, 0x33fff04f, 0x23016003, 0x608b2000,
        0xffcef7ff, 0x0500f8c1, 0xbf00bd08, 0x5004e000, 0xf242b538, 0x46140301, 0xf0214d0c, 0xf8c50103,
        0xf7ff3500, 0x4622ffbd, 0x1b004421, 0xeb02428a, 0xd1070300, 0x20002301, 0xf7ff60ab, 0xf8c5ffb1,
        0xbd380500, 0x4b04f852, 0xe7ef601c, 0x5004e000,
    ],

    # Relative function addresses
    'pc_init': 0x20000015,
    'pc_unInit': 0x2000001f,
    'pc_program_page': 0x20000075,
    'pc_erase_sector': 0x20000049,
    'pc_eraseAll': 0x20000025,

    'static_base' : 0x200000B8,
    'begin_stack' : 0x20003000,
    'page_size' : 0x1000,
    'analyzer_supported' : False,
    'analyzer_address' : 0x00000000,
    'page_buffers' : [0x20000100,0x20001100],   # 4 KiB double buffers, 128-bit line aligned
    'min_program_length' : 0x4,

    # Relative region addresses and sizes
    'ro_start': 0xb4,
    'ro_size': 0x0,
    'rw_start': 0xb4,
    'rw_size': 0x0,
    'zi_start': 0xb4,
    'zi_size': 0x0,

    # Flash information
    'flash_start': 0x0,
    'flash_size': 0x1fd000,
    'sector_sizes': (
        (0x0, 0x1fd000),
        (0xffd000, 0x1000),
    )
}
"""
_PYOCD_FLASH_ALGO_RE = re.compile(r"^FLASH_ALGO = \{.*?^\}\n", re.MULTILINE | re.DOTALL)
# An instruction word of the faulting algorithm only; an algorithm without it is left alone
_PYOCD_NRF54LM20A_FAULTY_WORD = "0x68134a02"


def _pyocd_builtin_target(python_env_path: Path, target_file: str) -> Path:
    site_packages = _get_penv_site_packages(python_env_path)
    if not site_packages.is_dir():
        # An env kept across an ESPHome Python upgrade still has its own Python version
        site_packages = next(
            python_env_path.glob("lib/python3.*/site-packages"), site_packages
        )
    return site_packages / "pyocd" / "target" / "builtin" / target_file


def _pyocd_nrf54lm20a_target(python_env_path: Path) -> Path:
    return _pyocd_builtin_target(python_env_path, "target_nRF54LM20A.py")


def pyocd_has_builtin_target(target_file: str) -> bool:
    """True when the SDK's pyOCD has this target file, e.g. target_nRF54L15.py."""
    env_path = _get_python_env_path(_get_version_str())
    return _pyocd_builtin_target(env_path, target_file).is_file()


def unpatched_pyocd_nrf54lm20a_target() -> Path | None:
    """PyOCD's nRF54LM20A target file when it has the faulting flash algorithm, else None."""
    target = _pyocd_nrf54lm20a_target(_get_python_env_path(_get_version_str()))
    if target.is_file() and _PYOCD_NRF54LM20A_FAULTY_WORD in target.read_text(
        encoding="utf-8"
    ):
        return target
    return None


def _patch_pyocd_nrf54lm20a_flash_algo(python_env_path: Path) -> None:
    target = _pyocd_nrf54lm20a_target(python_env_path)
    if not target.is_file():
        _LOGGER.debug(
            "pyOCD has no nRF54LM20A target at %s; not patching its flash algorithm",
            target,
        )
        return
    content = target.read_text(encoding="utf-8")
    if _PYOCD_NRF54LM20A_FAULTY_WORD not in content:
        return
    if (match := _PYOCD_FLASH_ALGO_RE.search(content)) is None:
        _LOGGER.warning(
            "Could not patch pyOCD's nRF54LM20A flash algorithm in %s", target
        )
        return
    patch_framework_file(target, match.group(0), _PYOCD_NRF54LM20A_FLASH_ALGO)
    _LOGGER.info("Patched pyOCD's nRF54LM20A flash algorithm in %s", target)


def _patch_uf2conv_escape_sequences(framework_path: Path) -> None:
    # SDK v2.6.1 ships uf2conv.py with '\s+' — an unrecognised escape that
    # Python 3.12+ flags with SyntaxWarning (a future version will reject it).
    uf2conv = framework_path / "zephyr" / "scripts" / "build" / "uf2conv.py"
    if uf2conv.exists():
        patch_framework_file(
            uf2conv, "re.split('\\s+', line)", "re.split('\\\\s+', line)"
        )


# West projects every build needs; components add others with include_west_project()
DEFAULT_WEST_PROJECTS = ("cmsis", "hal_nordic", "nrfxlib", "zephyr")

_KEY_NRF52 = "nrf52"
# The projects a finished install fetched
_WEST_PROJECTS_FILE = ".west_projects"


@dataclass
class _Nrf52Data:
    west_projects: set[str] = field(default_factory=lambda: set(DEFAULT_WEST_PROJECTS))
    variant: str = VARIANT_NRF52840
    board: str = ""


def _get_data() -> _Nrf52Data:
    if _KEY_NRF52 not in CORE.data:
        CORE.data[_KEY_NRF52] = _Nrf52Data()
    return CORE.data[_KEY_NRF52]


def set_nrf52_variant(variant: str) -> None:
    _get_data().variant = variant


def get_nrf52_variant() -> str:
    """The SoC variant, taken from the board name; NRF52840 until nrf52 is validated."""
    return _get_data().variant


def set_nrf52_board(board: str) -> None:
    _get_data().board = board


def get_nrf52_board() -> str:
    """The board name from the config; empty until nrf52 is validated."""
    return _get_data().board


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
    with install_lock(get_sdk_nrf_tools_path(), f"sdk-{version}"):
        _check_and_install(version)


def _check_and_install(version: str) -> None:
    python_env_path = _get_python_env_path(version)
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    install_venv = ensure_venv(python_env_path, _REQUIREMENTS, version)

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
    patch_gen_defines_dts_path(framework_path)
    patch_west_build_source_dir_relpath(framework_path)

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
    _patch_pyocd_nrf54lm20a_flash_algo(python_env_path)

    # Shared by every SDK version that uses the same toolchain; locked only
    # while missing
    toolchain_version = _get_toolchain_version()
    if not (_get_toolchain_path(toolchain_version) / ".ready").exists():
        with install_lock(get_sdk_nrf_tools_path(), f"toolchain-{toolchain_version}"):
            _install_toolchain()


# Nordic's nrfutil launcher, one static binary per host; its commands are installed from Nordic's
# package index by the launcher itself. The plain "nrfutil" file next to it is replaced with each
# release, so the versioned copy is downloaded and checked against the size and digest pinned here
NRFUTIL_LAUNCHER_VERSION = "1.4.1-f15ff58"
NORDIC_NRFUTIL_URL = (
    "https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/"
    "{triple}/nrfutil-{triple}-{version}{suffix}"
)
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
    sysname, machine, _ = get_sdk_host_info()
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
    with install_lock(get_sdk_nrf_tools_path(), "nrfutil"):
        if not installed():
            exe.parent.mkdir(parents=True, exist_ok=True)
            _LOGGER.info("Downloading Nordic's nrfutil ...")
            triple = _nrfutil_target_triple()
            size, sha256 = _NRFUTIL_LAUNCHERS[triple]
            download_with_resume(
                NORDIC_NRFUTIL_URL.format(
                    triple=triple, version=NRFUTIL_LAUNCHER_VERSION, suffix=suffix
                ),
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
        install_sdk(toolchains_dir, toolchain_version, ARM_TOOLCHAIN)
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
