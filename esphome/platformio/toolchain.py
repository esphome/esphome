from collections.abc import Iterable
import json
import logging
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import TYPE_CHECKING, Any

import platformdirs

from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME, KEY_CORE
from esphome.core import CORE, EsphomeError
from esphome.helpers import (
    add_git_ceiling_directory,
    copy_file_if_changed,
    get_bool_env,
    rmtree,
    write_file,
)
from esphome.util import ESP32_ARDUINO_ENV, FlashImage, run_external_process

if TYPE_CHECKING:
    from platformio.project.config import ProjectConfig

_LOGGER = logging.getLogger(__name__)

# PlatformIO cache subdirs resolved via ProjectConfig. A full ``clean-all`` wipes
# these plus the whole ``core_dir``; a Python-version heal wipes these plus the
# penv while keeping ``core_dir`` (so the sibling stamp/lock survive).
_PIO_CACHE_DIRS = ("cache_dir", "packages_dir", "platforms_dir")

# Marker recording the Python major.minor the PlatformIO cache was provisioned
# under, plus the lock guarding the check/wipe. Both live in the dir resolved
# by ``_pio_stamp_dir`` (NOT wiped by the heal), so they survive the wipe and
# are rewritten after it.
_PIO_PYTHON_STAMP_FILE = ".esphome.pio.stamp.json"
_PIO_PYTHON_STAMP_LOCK = ".esphome.pio.stamp.lock"
_PIO_PYTHON_STAMP_SCHEMA = "0"


def _strip_win_long_path_prefix(path: str) -> str:
    r"""Strip the Windows extended-length path prefix from ``path``.

    Handles both forms documented at
    https://learn.microsoft.com/windows/win32/fileio/naming-a-file:

    * ``\\?\C:\path\to\file`` -> ``C:\path\to\file``
    * ``\\?\UNC\server\share\path`` -> ``\\server\share\path``

    The NSIS-installed ``esphome.exe`` launcher on Windows starts Python with
    ``sys.executable`` already prefixed with ``\\?\``. That prefix propagates
    into PlatformIO's ``$PYTHONEXE`` (PlatformIO reads ``PYTHONEXEPATH`` from
    the environment, falling back to ``os.path.normpath(sys.executable)``)
    and ends up baked into SCons-emitted command lines for build steps such
    as the esp8266 ``elf2bin`` invocation. ``cmd.exe`` does not understand
    the ``\\?\`` prefix, so the build fails with
    "The system cannot find the path specified." Stripping the prefix early
    keeps the path shell-quotable.

    Also applied to the ccache path exported by ``_ccache_env()``, which
    ``shutil.which`` can return with the same prefix.

    No-op on non-Windows platforms.
    """
    if sys.platform != "win32":
        return path
    if path.startswith("\\\\?\\UNC\\"):
        # \\?\UNC\server\share\... -> \\server\share\...
        return "\\\\" + path[len("\\\\?\\UNC\\") :]
    if path.startswith("\\\\?\\"):
        return path[len("\\\\?\\") :]
    return path


def get_platformio_config() -> "ProjectConfig | None":
    """Return PlatformIO's ``ProjectConfig``, or None when PlatformIO is absent."""
    try:
        from platformio.project.config import ProjectConfig
    except ImportError:
        return None
    return ProjectConfig.get_instance()


def _pio_stamp_dir(config: "ProjectConfig") -> Path:
    """Return the persistent home for the python-version stamp and lock.

    The parent of ``platforms_dir``, not ``core_dir``: the container/add-on
    images relocate the platform/package caches to a persistent volume while
    ``core_dir`` stays at the ephemeral default (its ``appstate.json`` must not
    move), so a stamp under ``core_dir`` would be wiped on every image update
    while the stale cache it guards survives. Everywhere else ``platforms_dir``
    sits inside ``core_dir`` and this resolves to ``core_dir``.
    """
    return Path(config.get("platformio", "platforms_dir")).parent


def _delete_platformio_dirs(config: "ProjectConfig", pio_dirs: Iterable[str]) -> None:
    """Delete each named PlatformIO dir resolved from *config*."""
    for pio_dir in pio_dirs:
        path = Path(config.get("platformio", pio_dir))
        if path.is_dir():
            _LOGGER.info("Deleting PlatformIO %s %s", pio_dir, path)
            rmtree(path)


def clean_platformio_cache() -> None:
    """Wipe the whole PlatformIO cache (cache/packages/platforms/core).

    The full set ``clean-all`` (Reset Build Environment) clears. No-op when
    PlatformIO is unavailable.
    """
    config = get_platformio_config()
    if config is None:
        return
    _delete_platformio_dirs(config, [*_PIO_CACHE_DIRS, "core_dir"])


def _clean_platformio_python_env(config: "ProjectConfig", core_dir: Path) -> None:
    """Wipe the cache subdirs + penv for a Python-version change.

    Keeps ``core_dir`` itself (and the stamp/lock siblings under it); otherwise
    the same cache set ``clean-all`` clears.
    """
    _delete_platformio_dirs(config, _PIO_CACHE_DIRS)
    penv = core_dir / "penv"
    if penv.is_dir():
        _LOGGER.info("Deleting PlatformIO penv %s", penv)
        rmtree(penv)


def _current_python_minor() -> str:
    """Return the running interpreter's ``major.minor`` (e.g. ``3.13``)."""
    return f"{sys.version_info.major}.{sys.version_info.minor}"


def _read_pio_stamp_python(stamp_file: Path) -> str | None:
    """Return the ``python_version`` recorded in *stamp_file*, or None."""
    try:
        with stamp_file.open(encoding="utf-8") as f:
            data = json.load(f)
    except FileNotFoundError:
        return None
    except (json.JSONDecodeError, OSError) as err:
        # A present-but-unreadable stamp is a distinct signal from an absent
        # one, and it drives a cache clean; surface why at normal verbosity.
        _LOGGER.warning("Could not read %s: %s", stamp_file, err)
        return None
    if not isinstance(data, dict):
        return None
    version = data.get("python_version")
    return version if isinstance(version, str) else None


def _write_pio_stamp_python(stamp_file: Path, python_version: str) -> None:
    """Atomically write the PlatformIO python-version stamp."""
    write_file(
        stamp_file,
        json.dumps(
            {
                "schema_version": _PIO_PYTHON_STAMP_SCHEMA,
                "python_version": python_version,
            }
        ),
    )


def heal_platformio_python_env() -> None:
    """Wipe the PlatformIO cache unless it is stamped for the running Python.

    A PlatformIO platform/tool package pins the Python versions it accepts when
    it is provisioned, and ESPHome pins platforms to exact, immutable versions,
    so a later interpreter bump (a container upgrading its base Python) leaves
    the cached platform rejecting the new interpreter ("Python version must be
    between ...") until the cache is wiped. A stamp records the ``major.minor``
    the cache was provisioned for; when it doesn't match the running
    interpreter (or has never been written for an existing cache), the same
    PlatformIO dirs ``clean-all`` wipes are cleaned so PlatformIO
    re-provisions, matching Reset Build Environment automatically. The native
    ESP-IDF toolchain already self-heals through its own stamp; this covers the
    PlatformIO path. No-op when PlatformIO is unavailable.
    """
    config = get_platformio_config()
    if config is None:
        return
    try:
        _check_platformio_python_stamp(config)
    except (EsphomeError, OSError) as err:
        # The check is a best-effort repair; a full or read-only cache volume
        # must not abort a build that might otherwise work. The stamp write
        # surfaces as EsphomeError (write_file wraps OSError).
        _LOGGER.warning("PlatformIO build environment check failed: %s", err)


def _check_platformio_python_stamp(config: "ProjectConfig") -> None:
    """Compare the stamp to the running interpreter; wipe and restamp on mismatch."""
    current = _current_python_minor()
    stamp_dir = _pio_stamp_dir(config)
    # Host the stamp/lock even before PlatformIO's first run creates the dir.
    stamp_dir.mkdir(parents=True, exist_ok=True)
    stamp_file = stamp_dir / _PIO_PYTHON_STAMP_FILE

    from filelock import FileLock

    with FileLock(str(stamp_dir / _PIO_PYTHON_STAMP_LOCK)):
        provisioned = _read_pio_stamp_python(stamp_file)
        if provisioned == current:
            return
        core_dir = Path(config.get("platformio", "core_dir"))
        has_cache = (
            any(
                Path(config.get("platformio", pio_dir)).is_dir()
                for pio_dir in _PIO_CACHE_DIRS
            )
            or (core_dir / "penv").is_dir()
        )
        if has_cache:
            if provisioned is None:
                # An existing cache with no stamp predates the stamp: its
                # provisioning interpreter is unknown, so clean once rather
                # than leave a possibly-stale cache failing every build.
                _LOGGER.info(
                    "Cleaning the PlatformIO build environment once so it "
                    "re-provisions for Python %s",
                    current,
                )
            else:
                _LOGGER.info(
                    "Python version changed (%s -> %s); cleaning PlatformIO "
                    "build environment so it re-provisions for the new "
                    "interpreter",
                    provisioned,
                    current,
                )
            _clean_platformio_python_env(config, core_dir)
        _write_pio_stamp_python(stamp_file, current)


def _ccache_runs(ccache: str) -> bool:
    """Return True when the ``ccache`` found on PATH actually runs.

    ``shutil.which`` proves existence, not runnability: on Windows it also
    matches ``.bat``/``.cmd`` wrappers and stale package-manager shims whose
    target is gone. Wrapping compiles around such a find fails every compile
    step with an opaque OS error, so probe once and fall back to compiling
    without ccache when the probe fails.
    """
    try:
        subprocess.run(
            [ccache, "--version"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=15,
        )
    except (OSError, subprocess.SubprocessError):
        _LOGGER.warning(
            "Ignoring ccache at %s because it failed to run; compiling without ccache",
            ccache,
        )
        return False
    return True


def _ccache_env() -> dict[str, str]:
    r"""Return ccache settings for PlatformIO builds.

    Enabled by default whenever the ``ccache`` binary is on PATH; set
    ``ESPHOME_CCACHE_ENABLE=0`` in the environment to opt out (or ``1`` to
    force it on without the runnability probe; a binary is still needed).
    The decision is normalized into ``ESPHOME_CCACHE_ENABLE`` and the
    binary's location into ``ESPHOME_CCACHE_PATH`` so platform build scripts
    (the shared ``ccache.py`` extra script, which wraps compiler invocations
    inside SCons) only have to check for ``"1"`` and use the path as given
    instead of re-implementing the policy.

    The path is exported rather than looked up again inside SCons because
    ``shutil.which`` can return a Windows extended-length ``\\?\`` path
    (ESPHome Desktop puts its bundled ccache on PATH that way). Such a path
    runs fine through ``CreateProcess``, which is how ESP-IDF invokes it,
    but SCons runs every compile through ``cmd.exe``, which fails on it with
    "The system cannot find the path specified." (#18399), so the prefix is
    stripped here with ``_strip_win_long_path_prefix()`` before the
    runnability probe, which therefore validates the exact string the build
    will execute.
    ``ESPHOME_CCACHE_PATH`` is an internal channel, not a user setting: the
    script only honours it together with ``ESPHOME_CCACHE_ENABLE=1``, and this
    function always sets both or neither.

    The returned values are merged into the environment of the PlatformIO
    subprocess only, never into ``os.environ``: a long-running process
    (e.g. the dashboard) also runs ESP-IDF builds, whose own ccache setup
    skips defaults for ``CCACHE_*`` keys it finds already set, so leaking
    these values would hand it the wrong cache dir and a stale basedir.

    This mirrors ``_ccache_env()`` in ``esphome/espidf/framework.py``. The
    cache lives under the machine-global ESPHome cache dir, so it is shared
    across all projects and removed by ``esphome clean-all``. Unlike the
    ESP-IDF path, ``CCACHE_DEPEND`` is not set: SCons compiles don't emit
    the depfiles depend mode needs, so ccache's default preprocessor mode
    is used.

    ``CCACHE_BASEDIR`` rewrites the per-device absolute paths (the generated
    sources under src/, the .pioenvs build dir) so different devices with
    identical source share cache entries; it is always set to the current
    build dir. The other ``CCACHE_*`` values the user already set in the
    environment are respected.
    """
    explicit = "ESPHOME_CCACHE_ENABLE" in os.environ
    if explicit and not get_bool_env("ESPHOME_CCACHE_ENABLE"):
        return {"ESPHOME_CCACHE_ENABLE": "0"}
    ccache_path = shutil.which("ccache")
    if ccache_path is None:
        if explicit:
            _LOGGER.warning(
                "ESPHOME_CCACHE_ENABLE is set but no ccache binary is on PATH; "
                "compiling without ccache"
            )
        return {"ESPHOME_CCACHE_ENABLE": "0"}
    # Strip before probing so the probe validates (and the failure warning
    # names) the exact string the build will execute through cmd.exe.
    ccache_path = _strip_win_long_path_prefix(ccache_path)
    # An explicit opt-in skips the runnability probe.
    if not explicit and not _ccache_runs(ccache_path):
        return {"ESPHOME_CCACHE_ENABLE": "0"}
    env = {
        "ESPHOME_CCACHE_ENABLE": "1",
        "ESPHOME_CCACHE_PATH": ccache_path,
    }
    # build_path is set during preload for every config-loading command, so it
    # being unset means a caller built the environment too early; fail loudly
    # rather than with an opaque TypeError from Path(None).
    if CORE.build_path is None:
        raise ValueError(
            "CORE.build_path must be set before constructing the PlatformIO "
            "build environment"
        )
    env["CCACHE_BASEDIR"] = str(Path(CORE.build_path).resolve())
    defaults = {
        "CCACHE_DIR": str(
            Path(platformdirs.user_cache_dir("esphome", appauthor=False))
            / "platformio-ccache"
        ),
        "CCACHE_NOHASHDIR": "true",
    }
    env.update({k: v for k, v in defaults.items() if k not in os.environ})
    return env


def copy_ccache_script() -> None:
    """Copy the shared ccache SCons pre-script into the build dir.

    Platform components call this from their ``copy_files()`` and add
    ``pre:ccache.py`` to their ``extra_scripts``. The script wraps compiler
    invocations inside SCons with ccache; it is platform-agnostic, so it
    lives here next to ``_ccache_env()`` rather than being duplicated per
    component.
    """
    copy_file_if_changed(
        Path(__file__).parent / "ccache.py.script",
        CORE.relative_build_path("ccache.py"),
    )


def run_platformio_cli(*args, **kwargs) -> str | int:
    # Re-provision the PlatformIO cache if the interpreter's major.minor changed
    # since it was last built; a stale platform otherwise rejects the new Python
    # with "Python version must be between ..." until Reset Build Environment.
    heal_platformio_python_env()
    os.environ["PLATFORMIO_FORCE_COLOR"] = "true"
    os.environ["PLATFORMIO_BUILD_DIR"] = str(CORE.relative_pioenvs_path().absolute())
    os.environ.setdefault(
        "PLATFORMIO_LIBDEPS_DIR", str(CORE.relative_piolibdeps_path().absolute())
    )
    # Suppress Python syntax warnings from third-party scripts during compilation
    os.environ.setdefault("PYTHONWARNINGS", "ignore::SyntaxWarning")
    # Increase uv retry count to handle transient network errors (default is 3)
    os.environ.setdefault("UV_HTTP_RETRIES", "10")
    # Cap git's repo search at the config directory so the framework's build
    # scripts running `git describe` for the app version can't error out on an
    # uninitialized or corrupt git repo in a parent directory.
    add_git_ceiling_directory(os.environ, CORE.config_dir)
    # Strip the Windows extended-length path prefix from sys.executable so it
    # doesn't propagate into PlatformIO's $PYTHONEXE and break SCons-emitted
    # command lines run through cmd.exe.
    python_exe = _strip_win_long_path_prefix(sys.executable)
    if python_exe != sys.executable:
        # Only override PYTHONEXEPATH when we actually stripped a prefix.
        # PlatformIO's get_pythonexe_path() reads this and falls back to
        # sys.executable otherwise; setting it unconditionally would clobber
        # a user-provided value (or the unmodified path on platforms that
        # don't need the strip).
        os.environ["PYTHONEXEPATH"] = python_exe
    cmd = [python_exe, "-m", "esphome.platformio.runner"] + list(args)

    # ccache settings go into the subprocess environment only (see
    # _ccache_env() for why they must not leak into os.environ). A caller
    # supplied env is used as the base when present.
    base_env = kwargs.pop("env", None)
    env = dict(os.environ if base_env is None else base_env)
    env.update(_ccache_env())
    # The runner offers the out-of-flash tip but has no configured CORE, so
    # tell it. Ask CORE, not is_esp32_arduino_build(), which reads this same
    # variable; clear an inherited one so it cannot reach the wrong build.
    if CORE.is_configured and CORE.is_esp32 and CORE.using_arduino:
        env[ESP32_ARDUINO_ENV] = "1"
    else:
        env.pop(ESP32_ARDUINO_ENV, None)

    return run_external_process(*cmd, env=env, **kwargs)


def run_platformio_cli_run(config, verbose, *args, **kwargs) -> str | int:
    command = ["run", "-d", str(CORE.build_path)]
    if verbose:
        command += ["-v"]
    command += list(args)
    return run_platformio_cli(*command, **kwargs)


def run_compile(config, verbose):
    args = []
    if CONF_COMPILE_PROCESS_LIMIT in config[CONF_ESPHOME]:
        args += [f"-j{config[CONF_ESPHOME][CONF_COMPILE_PROCESS_LIMIT]}"]
    return run_platformio_cli_run(config, verbose, *args)


def _run_idedata(config):
    args = ["-t", "idedata"]
    stdout = run_platformio_cli_run(config, False, *args, capture_stdout=True)
    if not isinstance(stdout, str):
        # run_external_process returns 1 instead of captured output when
        # launching platformio raised; see the error it logged above.
        raise EsphomeError("Could not launch platformio to get idedata")
    match = re.search(r'{\s*".*}', stdout)
    if match is None:
        # A run that launches but fails emits its build error instead of
        # idedata; the logged stdout is the useful part, not a bug report.
        _LOGGER.error("Could not find idedata in the platformio output")
        _LOGGER.error("Stdout: %s", stdout)
        raise EsphomeError("PlatformIO did not report idedata")

    try:
        return json.loads(match.group())
    except ValueError as err:
        _LOGGER.exception("Could not parse idedata")
        _LOGGER.error("Stdout: %s", stdout)
        raise EsphomeError("Could not parse idedata from platformio") from err


def _load_idedata(config):
    platformio_ini = CORE.relative_build_path("platformio.ini")
    temp_idedata = CORE.relative_internal_path("idedata", f"{CORE.name}.json")

    changed = False
    if (
        not platformio_ini.is_file()
        or not temp_idedata.is_file()
        or platformio_ini.stat().st_mtime >= temp_idedata.stat().st_mtime
    ):
        changed = True

    if not changed:
        try:
            return json.loads(temp_idedata.read_text(encoding="utf-8"))
        except ValueError:
            pass

    temp_idedata.parent.mkdir(exist_ok=True, parents=True)

    data = _run_idedata(config)

    temp_idedata.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data


KEY_IDEDATA = "idedata"


def get_idedata(config) -> "IDEData":
    if KEY_IDEDATA in CORE.data[KEY_CORE]:
        return CORE.data[KEY_CORE][KEY_IDEDATA]
    idedata = IDEData(_load_idedata(config))
    CORE.data[KEY_CORE][KEY_IDEDATA] = idedata
    return idedata


class IDEData:
    def __init__(self, raw):
        self.raw = raw

    def _require(self, *keys: str) -> Any:
        """Read a nested key, classifying a miss as an environment error.

        A stale or truncated cached idedata JSON is the user's build
        tree, not a bug; recompiling regenerates it. The message names
        the key so a platformio schema change stays diagnosable.
        """
        value = self.raw
        # TypeError covers a key that is null instead of absent.
        try:
            for key in keys:
                value = value[key]
        except (KeyError, TypeError) as err:
            raise EsphomeError(
                f"Cached idedata is incomplete (missing {'.'.join(keys)})"
            ) from err
        return value

    @property
    def firmware_elf_path(self) -> Path:
        return Path(self._require("prog_path"))

    @property
    def firmware_bin_path(self) -> Path:
        return self.firmware_elf_path.with_suffix(".bin")

    @property
    def extra_flash_images(self) -> list[FlashImage]:
        try:
            return [
                FlashImage(path=Path(entry["path"]), offset=entry["offset"])
                for entry in self._require("extra", "flash_images")
            ]
        except (KeyError, TypeError) as err:
            # Covers entries missing path/offset and a null or non-list
            # flash_images value alike.
            raise EsphomeError(
                "Cached idedata is incomplete (malformed extra.flash_images)"
            ) from err

    @property
    def cc_path(self) -> str:
        # For example /Users/<USER>/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc
        return self._require("cc_path")

    @property
    def addr2line_path(self) -> str:
        # replace gcc at end with addr2line

        # Windows
        if self.cc_path.endswith(".exe"):
            return f"{self.cc_path[:-7]}addr2line.exe"

        return f"{self.cc_path[:-3]}addr2line"

    @property
    def objdump_path(self) -> str:
        # replace gcc at end with objdump
        path = self.cc_path
        return (
            f"{path[:-7]}objdump.exe"
            if path.endswith(".exe")
            else f"{path[:-3]}objdump"
        )

    @property
    def readelf_path(self) -> str:
        # replace gcc at end with readelf
        path = self.cc_path
        return (
            f"{path[:-7]}readelf.exe"
            if path.endswith(".exe")
            else f"{path[:-3]}readelf"
        )

    @property
    def defines(self) -> list[str]:
        """Return the list of preprocessor defines from idedata."""
        return self.raw.get("defines", [])
