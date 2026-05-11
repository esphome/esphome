"""Tests for the nrf52 component's custom upload_program."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

from esphome.core import CORE


def test_nrf52_upload_program_prebuilt_dir_for_ble_ota(tmp_path: Path) -> None:
    """`esphome upload --prebuilt-dir <path>` for an nRF52 Bluetooth proxy
    ships the MCUboot-signed update at <prebuilt-dir>/app_update.bin so the
    dashboard's transparent BLE install doesn't need a local Zephyr build
    tree. Verify the nrf52 upload_program picks up that path instead of
    `CORE.relative_pioenvs_path(..., "zephyr", "app_update.bin")`.
    """
    prebuilt = tmp_path / "prebuilt"
    prebuilt.mkdir()
    expected_firmware = prebuilt / "app_update.bin"
    expected_firmware.write_bytes(b"signed-mcuboot-image")
    CORE.prebuilt_dir = prebuilt
    CORE.name = "bt-proxy"

    captured_firmware: list[Path] = []

    async def fake_upload(device: str, firmware: Path) -> None:
        captured_firmware.append(firmware)

    # Patch the heavy nrf52 surface so we exercise only the firmware-path
    # resolution: BLE scan returns a fake device, smpmgr_upload records the
    # firmware path it would have flashed.
    with (
        patch(
            "esphome.components.nrf52.ota.smpmgr_scan",
            new=AsyncMock(return_value="AA:BB:CC:DD:EE:FF"),
        ),
        patch(
            "esphome.components.nrf52.ota.smpmgr_upload",
            new=fake_upload,
        ),
        patch(
            "esphome.components.nrf52.ble_logger.is_mac_address",
            return_value=False,
        ),
    ):
        from esphome.components.nrf52 import upload_program

        handled = upload_program({}, MagicMock(), "BLE")

    assert handled is True
    assert captured_firmware == [expected_firmware.resolve()]
