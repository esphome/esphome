import configparser
from contextlib import contextmanager
import hashlib
import json
import logging
import os
from pathlib import Path
import re
import sys

from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_HOST,
    PLATFORM_NRF52,
)
from esphome.core import CORE, EsphomeError
from esphome.util import FlashImage, run_external_process

_LOGGER = logging.getLogger(__name__)

_INI_AUTO_GENERATE_BEGIN = "; ========== AUTO GENERATED CODE BEGIN ==========="
_INI_AUTO_GENERATE_END = "; =========== AUTO GENERATED CODE END ============"
_PLATFORMIO_ENV_FINGERPRINT_OPTIONS = {
    "board",
    "framework",
    "lib_compat_mode",
    "lib_ldf_mode",
    "platform",
    "platform_packages",
}
_PLATFORMIO_MANAGED_LIBDEPS_ENV = "ESPHOME_PLATFORMIO_MANAGED_LIBDEPS_DIR"


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


def _platformio_toolchain_cache_key(core=CORE) -> str:
    """Return the stable key used for toolchain-scoped PlatformIO state."""
    core_data = core.data.get(KEY_CORE, {})
    platform = core_data.get(KEY_TARGET_PLATFORM) or "unknown"
    framework = core_data.get(KEY_TARGET_FRAMEWORK) or "unknown"
    return re.sub(r"[^a-zA-Z0-9_.-]", "_", f"{platform}-{framework}")


def _normalize_platformio_values(value: str | list[str] | None) -> list[str]:
    """Return sorted PlatformIO values for stable fingerprints."""
    if value is None:
        return []
    values = [value] if isinstance(value, str) else value
    return sorted({item for item in values if item})


def _read_common_platformio_lib_deps() -> list[str]:
    """Return preserved user lib_deps from the existing platformio.ini common section."""
    platformio_ini = CORE.relative_build_path("platformio.ini")
    if not platformio_ini.is_file():
        return []

    config = configparser.RawConfigParser(strict=False)
    try:
        config.read(platformio_ini, encoding="utf-8")
    except configparser.Error as err:
        _LOGGER.warning(
            "Could not parse %s for preserved common lib_deps: %s",
            platformio_ini,
            err,
        )
        return []

    if not config.has_option("common", "lib_deps"):
        return []

    return _normalize_platformio_values(config.get("common", "lib_deps").splitlines())


def _platformio_lib_dep_fingerprint_value(lib_dep: str, project_dir: Path) -> str:
    """Return a stable fingerprint value for one PlatformIO library dependency."""
    path = Path(lib_dep).expanduser()
    if path.is_absolute() or lib_dep.startswith((".", "~")):
        resolved = path if path.is_absolute() else project_dir / path
        return str(resolved.resolve())
    return lib_dep


def _platformio_libdeps_fingerprint_values() -> list[str]:
    """Return dependency values with project-local paths resolved for cache safety."""
    generated_lib_deps = [x.as_lib_dep for x in CORE.platformio_libraries.values()] + [
        "${common.lib_deps}"
    ]
    configured_lib_deps = CORE.platformio_options.get("lib_deps")
    if isinstance(configured_lib_deps, list):
        lib_deps = _normalize_platformio_values(
            configured_lib_deps + generated_lib_deps
        )
    else:
        lib_deps = _normalize_platformio_values(generated_lib_deps)

    project_dir = Path(CORE.build_path)
    values = []
    for lib_dep in lib_deps:
        if lib_dep == "${common.lib_deps}":
            values.extend(
                _platformio_lib_dep_fingerprint_value(common_lib_dep, project_dir)
                for common_lib_dep in _read_common_platformio_lib_deps()
            )
            continue

        values.append(_platformio_lib_dep_fingerprint_value(lib_dep, project_dir))
    return values


def _platformio_env_fingerprint_values() -> dict[str, object]:
    """Return values that must stay compatible inside one PlatformIO env."""
    platformio_options = {}
    for key, value in sorted(CORE.platformio_options.items()):
        if key == "lib_deps":
            continue
        if key in _PLATFORMIO_ENV_FINGERPRINT_OPTIONS or key.startswith("board_build."):
            platformio_options[key] = _normalize_platformio_values(value)

    return {
        "lib_deps": _platformio_libdeps_fingerprint_values(),
        "platformio_options": platformio_options,
    }


def _read_generated_platformio_env_name() -> str | None:
    """Return the env name from an existing generated platformio.ini."""
    platformio_ini = CORE.relative_build_path("platformio.ini")
    if not platformio_ini.is_file():
        return None

    text = platformio_ini.read_text(encoding="utf-8")
    begin = text.find(_INI_AUTO_GENERATE_BEGIN)
    end = text.find(_INI_AUTO_GENERATE_END)
    if begin != -1 and end != -1 and begin < end:
        text = text[begin + len(_INI_AUTO_GENERATE_BEGIN) : end]

    env_match = re.search(
        r"^\[env:([^\]]+)\]",
        text,
        flags=re.MULTILINE,
    )
    return env_match.group(1) if env_match else None


def platformio_env_name(*, prefer_existing: bool = False) -> str:
    """Return the generated PlatformIO env name for this build."""
    target_platform = CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM)
    if target_platform in (PLATFORM_HOST, PLATFORM_NRF52):
        return CORE.name

    if prefer_existing and (env_name := _read_generated_platformio_env_name()):
        return env_name

    fingerprint = hashlib.sha256(
        json.dumps(_platformio_env_fingerprint_values(), sort_keys=True).encode()
    ).hexdigest()[:16]
    return f"{_platformio_toolchain_cache_key()}-{fingerprint}"


def _platformio_libdeps_dir(core=CORE) -> Path:
    """Return a libdeps path shared by equivalent embedded PlatformIO configs."""
    target_platform = core.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM)
    if target_platform in (PLATFORM_HOST, PLATFORM_NRF52):
        return core.relative_piolibdeps_path()

    return core.relative_internal_path(
        "platformio", "libdeps", _platformio_toolchain_cache_key(core)
    )


def _platformio_uses_shared_libdeps(core=CORE) -> bool:
    """Return whether this target uses an ESPHome-managed shared libdeps root."""
    target_platform = core.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM)
    return target_platform not in (PLATFORM_HOST, PLATFORM_NRF52)


def _platformio_libdeps_lock_path() -> Path:
    """Return the lock path for the current shared PlatformIO libdeps env."""
    return CORE.relative_internal_path(
        "platformio", "libdeps", ".locks", f"{platformio_env_name()}.lock"
    )


def _set_platformio_libdeps_dir() -> Path | None:
    """Set ESPHome-managed libdeps dir while preserving user overrides."""
    libdeps_dir = str(_platformio_libdeps_dir().absolute())
    current = os.environ.get("PLATFORMIO_LIBDEPS_DIR")
    managed = os.environ.get(_PLATFORMIO_MANAGED_LIBDEPS_ENV)
    if current is not None and current != managed:
        return None

    os.environ["PLATFORMIO_LIBDEPS_DIR"] = libdeps_dir
    os.environ[_PLATFORMIO_MANAGED_LIBDEPS_ENV] = libdeps_dir
    if not _platformio_uses_shared_libdeps():
        return None
    return _platformio_libdeps_lock_path()


@contextmanager
def _platformio_libdeps_lock(lock_path: Path | None):
    """Hold a cross-process lock while PlatformIO mutates shared libdeps."""
    if lock_path is None:
        yield
        return

    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("a+b") as lock_file:
        if os.name == "nt":
            import msvcrt  # pylint: disable=import-error

            lock_file.seek(0)
            if not lock_file.read(1):
                lock_file.write(b"\0")
                lock_file.flush()
            lock_file.seek(0)
            msvcrt.locking(lock_file.fileno(), msvcrt.LK_LOCK, 1)
            try:
                yield
            finally:
                lock_file.seek(0)
                msvcrt.locking(lock_file.fileno(), msvcrt.LK_UNLCK, 1)
            return

        import fcntl

        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)


def run_platformio_cli(*args, **kwargs) -> str | int:
    os.environ["PLATFORMIO_FORCE_COLOR"] = "true"
    os.environ["PLATFORMIO_BUILD_DIR"] = str(CORE.relative_pioenvs_path().absolute())
    libdeps_lock_path = _set_platformio_libdeps_dir()
    # Suppress Python syntax warnings from third-party scripts during compilation
    os.environ.setdefault("PYTHONWARNINGS", "ignore::SyntaxWarning")
    # Increase uv retry count to handle transient network errors (default is 3)
    os.environ.setdefault("UV_HTTP_RETRIES", "10")
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

    with _platformio_libdeps_lock(libdeps_lock_path):
        return run_external_process(*cmd, **kwargs)


def run_platformio_cli_run(config, verbose, *args, **kwargs) -> str | int:
    command = [
        "run",
        "-d",
        str(CORE.build_path),
        "-e",
        platformio_env_name(prefer_existing=True),
    ]
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
