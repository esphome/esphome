"""Flashing over SWD with a SEGGER J-Link probe, such as the one built into Nordic DKs."""

from collections.abc import Iterator
import logging
import os
from pathlib import Path
import re
import shutil
import sys

from esphome.build_helpers.ccache import resolve_ccache_path
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import run_command_ok

from .const import VARIANT_NRF54L15
from .framework import (
    check_and_install,
    get_build_env,
    get_build_paths,
    get_nrf52_variant,
)

_LOGGER = logging.getLogger(__name__)

# J-Link device names for boards whose Zephyr runner args pass a generic core, which cannot program RRAM
_JLINK_DEVICES = {VARIANT_NRF54L15: "nRF54L15_M33"}

# Where SEGGER's installers put the J-Link tools when they are not on PATH
_JLINK_INSTALL_DIRS = {
    "linux": (Path("/opt/SEGGER"),),
    "darwin": (Path("/Applications/SEGGER"),),
    "win32": (
        Path(os.environ.get("PROGRAMFILES", r"C:\Program Files")) / "SEGGER",
        Path(os.environ.get("PROGRAMFILES(X86)", r"C:\Program Files (x86)")) / "SEGGER",
    ),
}


def _jlink_from_windows_registry() -> Path | None:
    try:
        import winreg  # pylint: disable=import-outside-toplevel,import-error

        with winreg.OpenKeyEx(
            winreg.HKEY_CURRENT_USER, r"Software\SEGGER\J-Link"
        ) as key:
            return Path(winreg.QueryValueEx(key, "InstallPath")[0]) / "JLink.exe"
    except OSError:
        return None


# The J-Link library that west's jlink runner loads from the commander's directory
# (JLink_x64.dll or JLinkARM.dll on Windows)
_JLINK_LIBRARY_GLOBS = {
    "linux": "libjlinkarm.so",
    "darwin": "libjlinkarm.dylib",
    "win32": "JLink*.dll",
}


def _jlink_version_key(commander: Path) -> tuple[int, str]:
    """Sort key for a versioned install folder, e.g. JLink_V812a -> (812, "a")."""
    match = re.fullmatch(r"JLink_V(\d+)(\w*)", commander.parent.name)
    return (int(match[1]), match[2]) if match else (-1, "")


def _jlink_commander_candidates(platform_key: str) -> Iterator[Path]:
    if platform_key == "win32":
        # PATH is not searched: the JDK's jlink.exe has the same name on a
        # case-insensitive file system
        exe = "JLink.exe"
        if (found := _jlink_from_windows_registry()) is not None:
            yield found
    else:
        exe = "JLinkExe"
        if (found := shutil.which(exe)) is not None:
            yield Path(found)
    for base in _JLINK_INSTALL_DIRS.get(platform_key, ()):
        # "JLink" is the unversioned link the Linux packages create; then the newest version
        yield base / "JLink" / exe
        yield from sorted(
            base.glob(f"JLink_V*/{exe}"), key=_jlink_version_key, reverse=True
        )


def find_jlink_commander() -> Path | None:
    """SEGGER's J-Link Commander, or None when the J-Link software is not installed."""
    platform_key = "linux" if sys.platform.startswith("linux") else sys.platform
    library = _JLINK_LIBRARY_GLOBS.get(platform_key, "libjlinkarm*")
    for candidate in _jlink_commander_candidates(platform_key):
        if not candidate.is_file():
            continue
        # The runner resolves links and loads the library next to the real file
        if any(candidate.resolve().parent.glob(library)):
            return candidate
        _LOGGER.warning(
            "Skipping %s: the J-Link library (%s) is not next to it",
            candidate,
            library,
        )
    return None


def upload_using_jlink() -> None:
    """Flash over SWD with a SEGGER J-Link probe, such as the one built into Nordic DKs."""
    if CORE.using_toolchain_platformio:
        raise EsphomeError("J-Link upload needs the 'sdk-nrf' toolchain")
    if (commander := find_jlink_commander()) is None:
        raise EsphomeError(
            "SEGGER J-Link software was not found. Install the J-Link Software and "
            "Documentation Pack from https://www.segger.com/downloads/jlink/ and try again."
        )
    check_and_install()
    paths = get_build_paths()
    env = get_build_env(resolve_ccache_path())  # west flash may rebuild
    west_cmd = [
        str(paths["python_executable"]),
        "-m",
        "west",
        "flash",
        "--runner",
        "jlink",
        "-d",
        str(CORE.relative_pioenvs_path(CORE.name)),
        "--commander",
        str(commander),
        # The runner only resumes the halted CPU unless asked to reset
        "--reset",
    ]
    if (device := _JLINK_DEVICES.get(get_nrf52_variant())) is not None:
        west_cmd += ["--device", device]
    if not run_command_ok(
        west_cmd, env=env, stream_output=True, cwd=str(paths["framework_path"])
    ):
        raise EsphomeError(
            "J-Link could not flash the device; the J-Link output above says why. "
            "Check that the board is powered and connected through its debugger "
            "USB port (the IMCU port on Nordic DKs). If the output says the chip "
            "is protected, it must be erased (recovered) once before flashing."
        )
