"""Toolchain utilities for memory analysis."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import subprocess
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Sequence

_LOGGER = logging.getLogger(__name__)

# Platform-specific toolchain prefixes
TOOLCHAIN_PREFIXES = [
    "xtensa-lx106-elf-",  # ESP8266
    "xtensa-esp32-elf-",  # ESP32
    "xtensa-esp-elf-",  # ESP32 (newer IDF)
    "arm-zephyr-eabi-",  # nRF52/Zephyr SDK
    "arm-none-eabi-",  # Generic ARM (RP2040, etc.)
    "",  # System default (no prefix)
]


def _find_in_platformio_packages(tool_name: str) -> str | None:
    """Search for a tool in PlatformIO package directories.

    This handles cases like Zephyr SDK where tools are installed in nested
    directories that aren't in PATH.

    Args:
        tool_name: Name of the tool (e.g., "readelf", "objdump")

    Returns:
        Full path to the tool or None if not found
    """
    # Get PlatformIO packages directory
    platformio_home = Path(os.path.expanduser("~/.platformio/packages"))
    if not platformio_home.exists():
        return None

    # Search patterns for toolchains that might contain the tool
    # Order matters - more specific patterns first
    search_patterns = [
        # Zephyr SDK deeply nested structure (4 levels)
        # e.g., toolchain-gccarmnoneeabi/zephyr-sdk-0.17.4/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump
        f"toolchain-*/*/*/bin/*-{tool_name}",
        # Zephyr SDK nested structure (3 levels)
        f"toolchain-*/*/bin/*-{tool_name}",
        f"toolchain-*/bin/*-{tool_name}",
        # Standard PlatformIO toolchain structure
        f"toolchain-*/bin/*{tool_name}",
    ]

    for pattern in search_patterns:
        matches = list(platformio_home.glob(pattern))
        if matches:
            # Sort to get consistent results, prefer arm-zephyr-eabi over arm-none-eabi
            matches.sort(key=lambda p: ("zephyr" not in str(p), str(p)))
            tool_path = str(matches[0])
            _LOGGER.debug("Found %s in PlatformIO packages: %s", tool_name, tool_path)
            return tool_path

    return None


def resolve_tool_path(
    tool_name: str,
    derived_path: str | None,
    objdump_path: str | None = None,
) -> str | None:
    """Resolve a tool path, falling back to find_tool if derived path doesn't exist.

    Args:
        tool_name: Name of the tool (e.g., "objdump", "readelf")
        derived_path: Path derived from idedata (may not exist for some platforms)
        objdump_path: Path to objdump binary to derive other tool paths from

    Returns:
        Resolved path to the tool, or the original derived_path if it exists
    """
    if derived_path and not Path(derived_path).exists():
        found = find_tool(tool_name, objdump_path)
        if found:
            _LOGGER.debug(
                "Derived %s path %s not found, using %s",
                tool_name,
                derived_path,
                found,
            )
            return found
    return derived_path


def find_tool(
    tool_name: str,
    objdump_path: str | None = None,
) -> str | None:
    """Find a toolchain tool by name.

    First tries to derive the tool path from objdump_path (if provided),
    then searches PlatformIO package directories (for cross-compile toolchains),
    and finally falls back to searching for platform-specific tools in PATH.

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

    # Search in PlatformIO packages directory first (handles Zephyr SDK, etc.)
    # This must come before PATH search because system tools (e.g., /usr/bin/objdump)
    # are for the host architecture, not the target (ARM, Xtensa, etc.)
    if found := _find_in_platformio_packages(tool_name):
        return found

    # Try platform-specific tools in PATH (fallback for when tools are installed globally)
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


def run_tool(
    cmd: Sequence[str],
    timeout: int = 30,
) -> subprocess.CompletedProcess[str] | None:
    """Run a toolchain command and return the result.

    Args:
        cmd: Command and arguments to run
        timeout: Timeout in seconds

    Returns:
        CompletedProcess on success, None on failure
    """
    try:
        return subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired:
        _LOGGER.warning("Command timed out: %s", " ".join(cmd))
        return None
    except FileNotFoundError:
        _LOGGER.warning("Command not found: %s", cmd[0])
        return None
    except OSError as e:
        _LOGGER.warning("Failed to run command %s: %s", cmd[0], e)
        return None
