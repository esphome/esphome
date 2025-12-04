"""Toolchain utilities for memory analysis."""

from __future__ import annotations

import logging
from pathlib import Path
import subprocess

_LOGGER = logging.getLogger(__name__)

# Platform-specific toolchain prefixes
TOOLCHAIN_PREFIXES = [
    "xtensa-lx106-elf-",  # ESP8266
    "xtensa-esp32-elf-",  # ESP32
    "xtensa-esp-elf-",  # ESP32 (newer IDF)
    "",  # System default (no prefix)
]


def find_tool(
    tool_name: str,
    objdump_path: str | None = None,
) -> str | None:
    """Find a toolchain tool by name.

    First tries to derive the tool path from objdump_path (if provided),
    then falls back to searching for platform-specific tools.

    Args:
        tool_name: Name of the tool (e.g., "objdump", "nm", "c++filt")
        objdump_path: Path to objdump binary to derive other tool paths from

    Returns:
        Path to the tool or None if not found
    """
    # Try to derive from objdump path first (most reliable)
    if objdump_path and objdump_path != "objdump":
        objdump_file = Path(objdump_path)
        # Replace just the filename portion, preserving any prefix (e.g., xtensa-esp32-elf-)
        new_name = objdump_file.name.replace("objdump", tool_name)
        potential_path = str(objdump_file.with_name(new_name))
        if Path(potential_path).exists():
            _LOGGER.debug("Found %s at: %s", tool_name, potential_path)
            return potential_path

    # Try platform-specific tools
    for prefix in TOOLCHAIN_PREFIXES:
        cmd = f"{prefix}{tool_name}"
        try:
            subprocess.run([cmd, "--version"], capture_output=True, check=True)
            _LOGGER.debug("Found %s: %s", tool_name, cmd)
            return cmd
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue

    _LOGGER.warning("Could not find %s tool", tool_name)
    return None
