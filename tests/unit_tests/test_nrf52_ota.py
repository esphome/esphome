"""Tests for nRF52 BLE device discovery and OTA transport selection."""

import asyncio
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from esphome.components.nrf52 import ble_logger, ota, show_logs
from esphome.core import EsphomeError

MAC = "AA:BB:CC:DD:EE:FF"
# macOS/CoreBluetooth identifies peripherals by UUID instead of MAC address
UUID = "0FA1B2C3-D4E5-F607-1829-3A4B5C6D7E8F"


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (MAC, True),
        (MAC.lower(), True),
        (UUID, True),
        ("/dev/ttyACM0", False),
        ("COM3", False),
    ],
)
def test_is_ble_address(value: str, expected: bool) -> None:
    assert ble_logger.is_ble_address(value) is expected


def _scan_result(
    monkeypatch, devices: list[tuple[str, str | None, str | None]]
) -> AsyncMock:
    """Stub the bleak scan so the filter is applied to the given (address, name, local_name) devices."""

    async def find_device_by_filter(filterfunc, timeout):
        for address, name, local_name in devices:
            device = SimpleNamespace(address=address, name=name)
            if filterfunc(device, SimpleNamespace(local_name=local_name)):
                return device
        return None

    mock = AsyncMock(side_effect=find_device_by_filter)
    monkeypatch.setattr(ble_logger.BleakScanner, "find_device_by_filter", mock)
    return mock


def test_scan_matches_advertised_local_name(monkeypatch) -> None:
    """On macOS device.name is cached from an earlier connection; the live name is adv.local_name."""
    _scan_result(monkeypatch, [(UUID, "stale-old-name", "my-device")])
    assert asyncio.run(ota.smpmgr_scan("my-device")) == UUID


def test_scan_matches_device_name(monkeypatch) -> None:
    _scan_result(monkeypatch, [(MAC, "my-device", None)])
    assert asyncio.run(ota.smpmgr_scan("my-device")) == MAC


def test_scan_raises_when_no_device_found(monkeypatch) -> None:
    _scan_result(monkeypatch, [(MAC, "someone-else", "someone-else")])
    with pytest.raises(EsphomeError, match="not found"):
        asyncio.run(ota.smpmgr_scan("my-device"))


@pytest.mark.parametrize(
    ("device", "transport"),
    [("/dev/ttyACM0", "serial"), ("COM3", "serial"), (MAC, "ble"), (UUID, "ble")],
)
def test_upload_transport(monkeypatch, device: str, transport: str) -> None:
    """Serial ports go to the serial transport; MAC addresses and macOS UUIDs to BLE."""
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
    monkeypatch.setattr(ota, "SMPSerialTransport", lambda: "serial")
    monkeypatch.setattr(ota, "SMPBLETransport", lambda: "ble")
    monkeypatch.setattr(ota, "SMPClient", FakeClient)

    asyncio.run(ota._smpmgr_upload(device, Path("firmware.bin")))  # pylint: disable=protected-access
    assert captured == {"transport": transport, "address": device}


@pytest.mark.parametrize(
    ("device", "connected_to"),
    [("BLE", UUID), (MAC, MAC), (UUID, UUID), ("my-device.local", None)],
)
def test_show_logs_routing(monkeypatch, device: str, connected_to: str | None) -> None:
    """A scanned device, a MAC or a UUID is handed to the BLE logger; anything else falls through."""
    scan = AsyncMock(return_value=SimpleNamespace(address=UUID))
    connect = AsyncMock(return_value=0)
    monkeypatch.setattr(ble_logger, "logger_scan", scan)
    monkeypatch.setattr(ble_logger, "logger_connect", connect)

    handled = show_logs(config={}, args=None, devices=[device])

    assert handled is (connected_to is not None)
    if connected_to is None:
        connect.assert_not_called()
    else:
        connect.assert_awaited_once_with(connected_to)


def test_show_logs_returns_when_the_scan_finds_nothing(monkeypatch) -> None:
    monkeypatch.setattr(ble_logger, "logger_scan", AsyncMock(return_value=None))
    connect = AsyncMock()
    monkeypatch.setattr(ble_logger, "logger_connect", connect)

    assert show_logs(config={}, args=None, devices=["BLE"]) is True
    connect.assert_not_called()
