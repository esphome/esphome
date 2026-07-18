from collections.abc import Iterable
import json
import logging
import os
from pathlib import Path
import re
import sys
from typing import TYPE_CHECKING

from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME, KEY_CORE
from esphome.core import CORE, EsphomeError
from esphome.helpers import add_git_ceiling_directory, rmtree, write_file
from esphome.util import FlashImage, run_external_process

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

    return run_external_process(*cmd, **kwargs)


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
    match = re.search(r'{\s*".*}', stdout)
    if match is None:
        _LOGGER.error("Could not match idedata, please report this error")
        _LOGGER.error("Stdout: %s", stdout)
        raise EsphomeError

    try:
        return json.loads(match.group())
    except ValueError:
        _LOGGER.exception("Could not parse idedata")
        _LOGGER.error("Stdout: %s", stdout)
        raise


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

    @property
    def firmware_elf_path(self) -> Path:
        return Path(self.raw["prog_path"])

    @property
    def firmware_bin_path(self) -> Path:
        return self.firmware_elf_path.with_suffix(".bin")

    @property
    def extra_flash_images(self) -> list[FlashImage]:
        return [
            FlashImage(path=Path(entry["path"]), offset=entry["offset"])
            for entry in self.raw["extra"]["flash_images"]
        ]

    @property
    def cc_path(self) -> str:
        # For example /Users/<USER>/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc
        return self.raw["cc_path"]

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
