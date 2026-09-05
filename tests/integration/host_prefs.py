"""Helpers for manipulating the host platform's preferences file.

ESPHome's host platform stores preferences in
``$ESPHOME_PREFDIR/<app_name>.prefs`` using a simple binary layout that
mirrors ``HostPreferences::sync()``:
``[uint32_t key][uint8_t len][uint8_t data[len]]`` per entry.

Tests use these helpers to pre-populate state the binary will see at
boot (e.g. forcing safe mode) or to clear stale state between runs.
"""

from __future__ import annotations

import os
from pathlib import Path
import struct


def host_prefs_path(device_name: str) -> Path:
    """Return the on-disk prefs file path for a host-platform device.

    Requires ESPHOME_PREFDIR, which the autouse isolated_preferences fixture
    sets; refusing the ~/.esphome/prefs fallback keeps tests off real user
    data if the fixture is ever bypassed."""
    prefdir = os.environ.get("ESPHOME_PREFDIR")
    if not prefdir:
        raise RuntimeError("ESPHOME_PREFDIR is not set; refusing the real prefs dir")
    return Path(prefdir) / f"{device_name}.prefs"


def clear_host_prefs(device_name: str) -> None:
    """Delete the prefs file for a host-platform device, if it exists."""
    host_prefs_path(device_name).unlink(missing_ok=True)


def write_host_prefs(device_name: str, entries: dict[int, bytes]) -> Path:
    """Write preference entries, replacing the file's contents.

    Returns the path that was written.
    """
    payload = b""
    for key, data in entries.items():
        if len(data) > 255:
            raise ValueError(f"Preference data too long: {len(data)} bytes (max 255)")
        payload += struct.pack("<IB", key, len(data)) + data
    path = host_prefs_path(device_name)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return path


def write_host_pref(device_name: str, key: int, data: bytes) -> Path:
    """Write a single preference entry, replacing the file's contents.

    Returns the path that was written.
    """
    return write_host_prefs(device_name, {key: data})
