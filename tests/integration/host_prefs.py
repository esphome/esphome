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

_ENTRY = struct.Struct("<IB")  # key, data length
# Must match esphome::safe_mode::RTC_KEY in safe_mode.h
_SAFE_MODE_RTC_KEY = 233825507
# Must match esphome::safe_mode::SafeModeComponent::ENTER_SAFE_MODE_MAGIC
_ENTER_SAFE_MODE_MAGIC = 0x5AFE5AFE


def host_prefs_path(device_name: str) -> Path:
    """Return the on-disk prefs file path for a host-platform device."""
    return Path.home() / ".esphome" / "prefs" / f"{device_name}.prefs"


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
        payload += _ENTRY.pack(key, len(data)) + data
    path = host_prefs_path(device_name)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return path


def read_host_prefs(device_name: str) -> dict[int, bytes]:
    """Read the preference entries of a host-platform device; empty when
    the file does not exist."""
    path = host_prefs_path(device_name)
    if not path.exists():
        return {}
    payload = path.read_bytes()
    entries: dict[int, bytes] = {}
    pos = 0
    while pos < len(payload):
        key, length = _ENTRY.unpack_from(payload, pos)
        pos += _ENTRY.size
        entries[key] = payload[pos : pos + length]
        pos += length
    return entries


def force_safe_mode(device_name: str) -> None:
    """Make the next boot of a host-platform device enter safe mode; other
    saved preferences are kept."""
    entries = read_host_prefs(device_name)
    entries[_SAFE_MODE_RTC_KEY] = struct.pack("<I", _ENTER_SAFE_MODE_MAGIC)
    write_host_prefs(device_name, entries)
