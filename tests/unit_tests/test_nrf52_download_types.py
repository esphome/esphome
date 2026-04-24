"""Tests for nRF52 dashboard firmware download choices."""

from pathlib import Path
from types import SimpleNamespace

from esphome.components import nrf52


def _storage_json(build_dir: Path) -> SimpleNamespace:
    return SimpleNamespace(
        name="test-device",
        firmware_bin_path=str(build_dir / "firmware.bin"),
    )


def test_nrf52_download_types_prefers_mcumgr_artifacts(tmp_path: Path) -> None:
    """MCUBoot builds include UF2 output but need HEX and mcumgr app downloads."""
    zephyr_dir = tmp_path / "zephyr"
    zephyr_dir.mkdir()
    (zephyr_dir / "zephyr.uf2").touch()
    (zephyr_dir / "merged.hex").touch()
    (zephyr_dir / "app_update.bin").touch()

    downloads = nrf52.get_download_types(_storage_json(tmp_path))

    assert downloads == [
        {
            "title": "HEX package",
            "description": "For initial flashing via pyocd using SWD.",
            "file": "zephyr/merged.hex",
            "download": "test-device.hex",
        },
        {
            "title": "App update package",
            "description": "For flashing via mcumgr-web using BLE or smpclient using USB CDC.",
            "file": "zephyr/app_update.bin",
            "download": "app-test-device.img",
        },
    ]


def test_nrf52_download_types_keeps_adafruit_uf2_default(tmp_path: Path) -> None:
    """Adafruit bootloader builds keep their existing UF2 and DFU choices."""
    zephyr_dir = tmp_path / "zephyr"
    zephyr_dir.mkdir()
    (zephyr_dir / "zephyr.uf2").touch()

    downloads = nrf52.get_download_types(_storage_json(tmp_path))

    assert downloads == [
        {
            "title": "UF2 package (recommended)",
            "description": "For flashing via Adafruit nRF52 Bootloader as a flash drive.",
            "file": "zephyr/zephyr.uf2",
            "download": "test-device.uf2",
        },
        {
            "title": "DFU package",
            "description": "For flashing via adafruit-nrfutil using USB CDC.",
            "file": "firmware.zip",
            "download": "dfu-test-device.zip",
        },
    ]
