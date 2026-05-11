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


def prepare_platform_for_upload(config, verbose) -> str | int:
    """Configure the PlatformIO build environment for ``CORE.name`` without
    compiling, so platform-specific flashing tools end up on disk.

    Used by ``esphome upload --prebuilt-dir`` on hosts that have never
    compiled the target platform locally. ``pio pkg install`` alone isn't
    enough on libretiny: the platform package downloads cleanly, but
    ``ltchiptool`` lives in a platform-managed virtualenv at
    ``~/.platformio/penv/.libretiny/`` that's only created by libretiny's
    ``ConfigurePythonVenv`` SCons step, which runs during ``pio run`` (not
    ``pio pkg install``). So we run ``pio run -t idedata`` instead: it
    triggers SConscript -- creating the penv on libretiny, installing the
    picotool tool package on RP2040 -- but skips the actual compile target
    so this is much cheaper than a full build.

    The idedata JSON gets emitted to stdout as a side effect of the
    target; we don't filter it out -- the install runs once per cold host
    and the trailing JSON blob is harmless noise.
    """
    return run_platformio_cli_run(config, verbose, "-t", "idedata")


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


def _resolve_prebuilt_idedata_paths(data: dict, prebuilt_dir: Path) -> None:
    """Resolve relative paths in a prebuilt idedata.json against prebuilt_dir.

    The dashboard's transparent-install pipeline rewrites ``prog_path`` and
    ``extra.flash_images[*].path`` to bare basenames before shipping the
    tarball (the receiver's build-host absolute paths don't resolve on the
    offloader). Accept both shapes: absolute paths pass through unchanged
    so a hand-built --prebuilt-dir with absolute paths still works, and
    bare basenames or other relative paths resolve to ``prebuilt_dir / p``.

    Mutates ``data`` in place. ``cc_path`` is left alone because it points
    at a PlatformIO toolchain binary (~/.platformio/packages/...) that
    lives outside the prebuilt dir; the offloader's local PIO install
    provides the matching binary by virtue of running on the same machine
    as ``esphome upload``.
    """
    prog = data.get("prog_path")
    if prog is not None and not Path(prog).is_absolute():
        data["prog_path"] = str(prebuilt_dir / prog)

    extra = data.get("extra")
    if isinstance(extra, dict):
        for image in extra.get("flash_images", []) or []:
            path = image.get("path") if isinstance(image, dict) else None
            if path is not None and not Path(path).is_absolute():
                image["path"] = str(prebuilt_dir / path)


def _load_idedata(config):
    # `esphome upload --prebuilt-dir` ships a pre-rendered idedata.json next
    # to the artifacts. When present we use it as-is, with one rewrite pass:
    # ``prog_path`` and ``extra.flash_images[*].path`` may be either absolute
    # paths (hand-built directories) or bare basenames (the dashboard's wire
    # format) -- relative paths are resolved against ``CORE.prebuilt_dir`` so
    # both shapes work without the dashboard having to write a fresh
    # idedata.json with absolute paths on every install.
    #
    # No schema validation or referenced-path existence check happens here;
    # a missing path inside the idedata will surface later as a "file not
    # found" from esptool / picotool. A malformed JSON file is caught here
    # and surfaced as EsphomeError so the failure mode is a one-line
    # diagnostic instead of an unhandled JSONDecodeError stack trace.
    if CORE.prebuilt_dir is not None:
        prebuilt_idedata = CORE.prebuilt_dir / "idedata.json"
        if prebuilt_idedata.is_file():
            try:
                data = json.loads(prebuilt_idedata.read_text(encoding="utf-8"))
            except json.JSONDecodeError as err:
                raise EsphomeError(
                    f"Failed to parse {prebuilt_idedata}: {err}. The dashboard "
                    "must stage a syntactically valid idedata.json under "
                    "--prebuilt-dir; the upload cannot proceed without it."
                ) from err
            _resolve_prebuilt_idedata_paths(data, CORE.prebuilt_dir)
            return data

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
