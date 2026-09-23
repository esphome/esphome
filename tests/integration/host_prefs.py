"""Helpers for manipulating the host platform's preferences file.

ESPHome's host platform stores preferences in
``~/.esphome/prefs/<app_name>.prefs`` using a simple binary layout that
mirrors ``HostPreferences::sync()``:
``[uint32_t key][uint8_t len][uint8_t data[len]]`` per entry.

Tests use these helpers to pre-populate state the binary will see at
boot (e.g. forcing safe mode) or to clear stale state between runs.
"""

from __future__ import annotations

from pathlib import Path
import struct


def host_prefs_path(device_name: str) -> Path:
    """Return the on-disk prefs file path for a host-platform device."""
    return Path.home() / ".esphome" / "prefs" / f"{device_name}.prefs"


def clear_host_prefs(device_name: str) -> None:
    """Delete the prefs file for a host-platform device, if it exists."""
    host_prefs_path(device_name).unlink(missing_ok=True)


def write_host_pref(device_name: str, key: int, data: bytes) -> Path:
    """Write a single preference entry, replacing the file's contents.

    Returns the path that was written.
    """
    if len(data) > 255:
        raise ValueError(f"Preference data too long: {len(data)} bytes (max 255)")
    path = host_prefs_path(device_name)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = struct.pack("<IB", key, len(data)) + data
    path.write_bytes(payload)
    return path
