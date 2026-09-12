"""Tests for nRF52 BLE OTA discovery and transport selection.

Lock in the macOS-specific fixes:
  * smpmgr_scan() matches the live advertised name as well as the (possibly
    stale) GAP device name, prefers a device that advertises the SMP/OTA
    service UUID, and falls back to the name match when the UUID is not
    visible in the scan response.
  * _smpmgr_upload() routes by port type: serial transport only for
    filesystem paths (/dev/..., COMx), BLE transport for everything else
    (macOS identifies BLE peripherals by a CoreBluetooth UUID, not a MAC).
"""

import asyncio
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from esphome.components.nrf52 import ota
from esphome.core import EsphomeError

SMP_UUID = ota.SMP_SERVICE_UUID.lower()


def _device(address: str, name: str | None = None) -> SimpleNamespace:
    return SimpleNamespace(address=address, name=name)


def _adv(
    local_name: str | None = None, service_uuids: list[str] | None = None
) -> SimpleNamespace:
    return SimpleNamespace(local_name=local_name, service_uuids=service_uuids or [])


def _run_scan(monkeypatch, discovered: dict, name: str = "my-device") -> str:
    monkeypatch.setattr(
        ota.BleakScanner, "discover", AsyncMock(return_value=discovered)
    )
    return asyncio.run(ota.smpmgr_scan(name))


def test_scan_matches_advertised_local_name(monkeypatch) -> None:
    """macOS: device.name is a stale cached name, the live name is in adv.local_name."""
    discovered = {
        "AA:BB": (
            _device("AA:BB", name="stale-old-name"),
            _adv(local_name="my-device"),
        ),
    }
    assert _run_scan(monkeypatch, discovered) == "AA:BB"


def test_scan_matches_device_name(monkeypatch) -> None:
    """The cached GAP device name still counts as a match when local_name is absent."""
    discovered = {
        "CC:DD": (_device("CC:DD", name="my-device"), _adv(local_name=None)),
    }
    assert _run_scan(monkeypatch, discovered) == "CC:DD"


def test_scan_prefers_device_advertising_smp_uuid(monkeypatch) -> None:
    """When the SMP UUID is visible (e.g. BlueZ) prefer it over a name-only fallback."""
    discovered = {
        "AA:AA": (_device("AA:AA", name="my-device"), _adv(local_name="my-device")),
        "BB:BB": (
            _device("BB:BB", name="my-device"),
            # advertised in upper case to confirm case-insensitive UUID matching
            _adv(local_name="my-device", service_uuids=[ota.SMP_SERVICE_UUID]),
        ),
    }
    assert _run_scan(monkeypatch, discovered) == "BB:BB"


def test_scan_ignores_uuid_when_name_does_not_match(monkeypatch) -> None:
    """A device advertising the SMP UUID under a different name is not our target."""
    discovered = {
        "EE:EE": (
            _device("EE:EE", name="someone-else"),
            _adv(local_name="someone-else", service_uuids=[SMP_UUID]),
        ),
    }
    with pytest.raises(EsphomeError, match="OTA service not found"):
        _run_scan(monkeypatch, discovered)


def test_scan_raises_when_no_device_found(monkeypatch) -> None:
    with pytest.raises(EsphomeError, match="OTA service not found"):
        _run_scan(monkeypatch, {})


def _capture_upload_transport(monkeypatch, device: str):
    """Run _smpmgr_upload with the SMP machinery stubbed, return the transport used."""
    serial_sentinel = object()
    ble_sentinel = object()
    captured: dict = {}

    class FakeClient:
        def __init__(self, transport, address):
            captured["transport"] = transport
            captured["address"] = address

        async def connect(self) -> None:
            pass

        async def disconnect(self) -> None:
            pass

    monkeypatch.setattr(ota, "_get_image_tlv_sha256", lambda firmware: b"")
    monkeypatch.setattr(ota, "_smpmgr_upload_connected", AsyncMock())
    monkeypatch.setattr(ota, "SMPSerialTransport", lambda: serial_sentinel)
    monkeypatch.setattr(ota, "SMPBLETransport", lambda: ble_sentinel)
    monkeypatch.setattr(ota, "SMPClient", FakeClient)

    asyncio.run(ota._smpmgr_upload(device, Path("firmware.bin")))
    return captured, serial_sentinel, ble_sentinel


def test_upload_routes_serial_path_to_serial_transport(monkeypatch) -> None:
    captured, serial_sentinel, _ = _capture_upload_transport(
        monkeypatch, "/dev/ttyACM0"
    )
    assert captured["transport"] is serial_sentinel
    assert captured["address"] == "/dev/ttyACM0"


def test_upload_routes_ble_handle_to_ble_transport(monkeypatch) -> None:
    """A macOS CoreBluetooth UUID is not a serial path -> BLE transport."""
    ble_handle = "0FA1B2C3-D4E5-F607-1829-3A4B5C6D7E8F"
    captured, _, ble_sentinel = _capture_upload_transport(monkeypatch, ble_handle)
    assert captured["transport"] is ble_sentinel
    assert captured["address"] == ble_handle
