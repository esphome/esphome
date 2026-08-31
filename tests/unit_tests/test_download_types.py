"""Platform get_download_types contract for never-built configs.

Wizard-written and upload/logs-fallback sidecars record no
firmware_bin_path; the download panel must get an empty list for them,
not entries pointing at files that were never built.
"""

from __future__ import annotations

from importlib import import_module
from pathlib import Path
from typing import Any

import pytest

from esphome.storage_json import StorageJSON

PLATFORMS = ["esp32", "esp8266", "rp2", "libretiny", "nrf52"]


def _download_types(platform: str, storage: StorageJSON) -> list[dict[str, Any]]:
    return import_module(f"esphome.components.{platform}").get_download_types(storage)


def _wizard_storage() -> StorageJSON:
    return StorageJSON.from_wizard(
        name="test_device",
        friendly_name="Test Device",
        address="test_device.local",
        platform="ESP32",
    )


@pytest.mark.parametrize("platform", PLATFORMS)
def test_no_firmware_path_yields_no_downloads(platform: str) -> None:
    """No recorded firmware path means nothing was built; no downloads."""
    assert _download_types(platform, _wizard_storage()) == []


@pytest.mark.parametrize("platform", PLATFORMS)
def test_recorded_firmware_path_yields_downloads(platform: str, tmp_path: Path) -> None:
    """With a firmware path recorded, every platform offers entries in
    the documented title/description/file/download shape."""
    storage = _wizard_storage()
    storage.firmware_bin_path = tmp_path / "firmware.bin"

    types = _download_types(platform, storage)

    assert types
    assert all(
        {"title", "description", "file", "download"} <= entry.keys() for entry in types
    )
