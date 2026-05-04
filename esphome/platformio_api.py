import json
import logging
import os
from pathlib import Path
import re
import sys

from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME, KEY_CORE
from esphome.core import CORE, EsphomeError
from esphome.util import FlashImage, run_external_process

_LOGGER = logging.getLogger(__name__)


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


def run_platformio_cli(*args, **kwargs) -> str | int:
    os.environ["PLATFORMIO_FORCE_COLOR"] = "true"
    os.environ["PLATFORMIO_BUILD_DIR"] = str(CORE.relative_pioenvs_path().absolute())
    os.environ.setdefault(
        "PLATFORMIO_LIBDEPS_DIR", str(CORE.relative_piolibdeps_path().absolute())
    )
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
    cmd = [python_exe, "-m", "esphome.platformio_runner"] + list(args)

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
        _LOGGER.error("Could not parse idedata", exc_info=True)
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
