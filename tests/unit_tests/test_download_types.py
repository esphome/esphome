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


def test_esp32_factory_entry_requires_the_file(tmp_path: Path) -> None:
    """A --skip-bootloader build has no factory image; do not offer one."""
    storage = _wizard_storage()
    storage.firmware_bin_path = tmp_path / "firmware.bin"

    files = [entry["file"] for entry in _download_types("esp32", storage)]
    assert files == ["firmware.ota.bin"]

    (tmp_path / "firmware.factory.bin").touch()
    files = [entry["file"] for entry in _download_types("esp32", storage)]
    assert files == ["firmware.factory.bin", "firmware.ota.bin"]


def _nrf52_files(tmp_path: Path, *built: str) -> list[str]:
    """The files nrf52 offers for a build directory holding *built*."""
    (tmp_path / "zephyr").mkdir()
    for name in built:
        (tmp_path / name).touch()
    storage = _wizard_storage()
    storage.firmware_bin_path = tmp_path / "firmware.bin"
    return [entry["file"] for entry in _download_types("nrf52", storage)]


@pytest.mark.parametrize(
    ("built", "expected"),
    [
        # Adafruit bootloader with the mcumgr OTA: MCUboot is chained behind it.
        (
            ["zephyr/zephyr.uf2", "zephyr/app_update.bin"],
            ["zephyr/zephyr.uf2", "firmware.zip", "zephyr/app_update.bin"],
        ),
        (["zephyr/zephyr.uf2"], ["zephyr/zephyr.uf2", "firmware.zip"]),
        (
            ["zephyr/merged.hex", "zephyr/app_update.bin"],
            ["zephyr/merged.hex", "zephyr/app_update.bin"],
        ),
        (["zephyr/merged.hex"], ["zephyr/merged.hex"]),
        ([], ["zephyr/zephyr.hex"]),
    ],
)
def test_nrf52_offers_the_mcumgr_image_whenever_it_was_built(
    tmp_path: Path, built: list[str], expected: list[str]
) -> None:
    """The mcumgr update image is offered beside a UF2 as well as beside a HEX."""
    assert _nrf52_files(tmp_path, *built) == expected
