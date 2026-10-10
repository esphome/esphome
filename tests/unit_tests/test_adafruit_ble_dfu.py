"""Tests for the Adafruit bootloader BLE DFU client."""

import asyncio
import json
from pathlib import Path
import zipfile

import pytest

from esphome.components.adafruit_ble import dfu
from esphome.core import EsphomeError


def _write_package(
    directory: Path, image: bytes = b"\x01\x02", init: bytes = b"\x03"
) -> Path:
    package = directory / "firmware.zip"
    with zipfile.ZipFile(package, "w") as archive:
        archive.writestr(
            "manifest.json",
            json.dumps(
                {
                    "manifest": {
                        "application": {"bin_file": "app.bin", "dat_file": "app.dat"}
                    }
                }
            ),
        )
        archive.writestr("app.bin", image)
        archive.writestr("app.dat", init)
    return package


def _expects_response(data: bytes) -> bool:
    """The bootloader answers one logical command, not every control point write."""
    if data[0] == dfu._OP_INIT_DFU_PARAMS:
        # The init packet is sent as two writes but answered once, after the second
        return len(data) > 1 and data[1] == 0x01
    return data[0] in (
        dfu._OP_START_DFU,
        dfu._OP_RECEIVE_FIRMWARE_IMAGE,
        dfu._OP_VALIDATE,
        dfu._OP_RESET,
    )


class _FakeBleClient:
    """Answers control point writes with a DFU response."""

    def __init__(self, mtu_size: int = 23, status: int = 0x01) -> None:
        self.writes: list[tuple[str, bytes, bool]] = []
        self.handler = None
        self.mtu_size = mtu_size
        self.status = status

    async def start_notify(self, characteristic: str, handler) -> None:
        self.handler = handler

    async def stop_notify(self, characteristic: str) -> None:
        pass

    async def write_gatt_char(
        self, characteristic: str, data, response: bool = True
    ) -> None:
        data = bytes(data)
        self.writes.append((characteristic, data, response))
        if characteristic == dfu.DFU_CONTROL_POINT_UUID and _expects_response(data):
            self.handler(None, bytearray([dfu._OP_RESPONSE_CODE, data[0], self.status]))

    def control_point_ops(self) -> list[int]:
        return [
            data[0]
            for characteristic, data, _ in self.writes
            if characteristic == dfu.DFU_CONTROL_POINT_UUID
        ]

    def packet_writes(self) -> list[bytes]:
        return [
            data
            for characteristic, data, _ in self.writes
            if characteristic == dfu.DFU_PACKET_UUID
        ]


class _ReceiptFakeBleClient(_FakeBleClient):
    """Also reports a packet receipt notification for every packet."""

    async def write_gatt_char(
        self, characteristic: str, data, response: bool = True
    ) -> None:
        await super().write_gatt_char(characteristic, data, response)
        if characteristic == dfu.DFU_PACKET_UUID:
            asyncio.get_running_loop().call_soon(
                self.handler,
                None,
                bytearray([dfu._OP_PACKET_RECEIPT_NOTIF, 0, 0, 0, 0]),
            )


@pytest.mark.parametrize(
    ("address", "expected"),
    [
        ("AA:BB:CC:DD:EE:FE", "AA:BB:CC:DD:EE:FF"),
        ("AA:BB:CC:DD:EE:FF", "AA:BB:CC:DD:EE:00"),
        ("not-an-address", None),
    ],
)
def test_next_address(address: str, expected: str | None) -> None:
    assert dfu._next_address(address) == expected


def test_load_firmware_from_manifest(tmp_path: Path) -> None:
    image, init_packet, size = dfu._load_firmware(_write_package(tmp_path))
    assert image == b"\x01\x02"
    assert init_packet == b"\x03"
    assert size == 2


def test_load_firmware_without_manifest(tmp_path: Path) -> None:
    package = tmp_path / "firmware.zip"
    with zipfile.ZipFile(package, "w") as archive:
        archive.writestr("app.bin", b"\xaa")
        archive.writestr("app.dat", b"\xbb")
    image, init_packet, size = dfu._load_firmware(package)
    assert (image, init_packet, size) == (b"\xaa", b"\xbb", 1)


def test_load_firmware_missing_file(tmp_path: Path) -> None:
    with pytest.raises(EsphomeError, match="not found"):
        dfu._load_firmware(tmp_path / "nope.zip")


def test_load_firmware_without_application(tmp_path: Path) -> None:
    package = tmp_path / "firmware.zip"
    with zipfile.ZipFile(package, "w") as archive:
        archive.writestr("manifest.json", json.dumps({"manifest": {"softdevice": {}}}))
    with pytest.raises(EsphomeError, match="not an application update package"):
        dfu._load_firmware(package)


@pytest.mark.asyncio
async def test_perform_update_opcode_sequence(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(dfu, "_START_DELAY", 0)
    monkeypatch.setattr(dfu, "_PRN", 1000)
    client = _FakeBleClient()

    await dfu._perform_update(client, b"\x00" * 100, b"\x01\x02")

    assert client.control_point_ops() == [
        dfu._OP_START_DFU,
        dfu._OP_INIT_DFU_PARAMS,
        dfu._OP_INIT_DFU_PARAMS,
        dfu._OP_PACKET_RECEIPT_NOTIF_REQ,
        dfu._OP_RECEIVE_FIRMWARE_IMAGE,
        dfu._OP_VALIDATE,
        dfu._OP_ACTIVATE_AND_RESET,
    ]
    packets = client.packet_writes()
    assert packets[0] == (0).to_bytes(4, "little") * 2 + (100).to_bytes(4, "little")
    assert packets[1] == b"\x01\x02"
    # 100 bytes split into 20 byte chunks (MTU 23 -> 20 byte ATT payload)
    assert packets[2:] == [b"\x00" * 20] * 5


@pytest.mark.asyncio
async def test_perform_update_reports_failure(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(dfu, "_START_DELAY", 0)
    client = _FakeBleClient(status=0x04)

    with pytest.raises(EsphomeError, match="DFU start failed"):
        await dfu._perform_update(client, b"\x00" * 4, b"\x01")


@pytest.mark.asyncio
async def test_stream_firmware_waits_for_packet_receipt(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(dfu, "_PRN", 2)
    client = _ReceiptFakeBleClient()
    session = dfu._DfuSession()
    client.handler = session.notification_handler

    await dfu._stream_firmware(client, session, b"\x00" * 100)

    assert len(client.packet_writes()) == 5
    assert session.packet_receipt.is_set()


@pytest.mark.asyncio
async def test_request_dfu_mode_writes_magic(monkeypatch: pytest.MonkeyPatch) -> None:
    class _Client:
        def __init__(self) -> None:
            self.writes: list[tuple[str, bytes]] = []

        async def connect(self) -> None:
            pass

        async def disconnect(self) -> None:
            pass

        async def write_gatt_char(self, characteristic, data, response=True) -> None:
            self.writes.append((characteristic, bytes(data)))

    client = _Client()
    monkeypatch.setattr(dfu, "BleakClient", lambda *args, **kwargs: client)

    await dfu.request_dfu_mode("AA:BB:CC:DD:EE:FF")

    assert client.writes == [(dfu.TRIGGER_CHARACTERISTIC_UUID, dfu.TRIGGER_MAGIC)]


@pytest.mark.asyncio
async def test_request_dfu_mode_tolerates_dropped_link(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The reboot drops the link, so the write may fail instead of being acked."""

    class _Client:
        def __init__(self) -> None:
            self.writes: list[tuple[str, bytes]] = []

        async def connect(self) -> None:
            pass

        async def disconnect(self) -> None:
            pass

        async def write_gatt_char(self, characteristic, data, response=True) -> None:
            self.writes.append((characteristic, bytes(data)))
            # Windows reports the cancelled operation as an OSError
            raise OSError(-2147023673, "The operation was canceled by the user.")

    client = _Client()
    monkeypatch.setattr(dfu, "BleakClient", lambda *args, **kwargs: client)

    await dfu.request_dfu_mode("AA:BB:CC:DD:EE:FF")

    assert client.writes == [(dfu.TRIGGER_CHARACTERISTIC_UUID, dfu.TRIGGER_MAGIC)]


@pytest.mark.asyncio
async def test_perform_update_tolerates_dropped_link_on_activate(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The reset after a successful upload drops the link mid-write."""

    class _Client(_FakeBleClient):
        async def write_gatt_char(
            self, characteristic, data, response: bool = True
        ) -> None:
            if (
                characteristic == dfu.DFU_CONTROL_POINT_UUID
                and data[0] == dfu._OP_ACTIVATE_AND_RESET
            ):
                self.writes.append((characteristic, bytes(data), response))
                raise OSError(-2147023673, "The operation was canceled by the user.")
            await super().write_gatt_char(characteristic, data, response)

    monkeypatch.setattr(dfu, "_START_DELAY", 0)
    monkeypatch.setattr(dfu, "_PRN", 1000)
    client = _Client()

    await dfu._perform_update(client, b"\x00" * 40, b"\x01")

    assert client.control_point_ops()[-1] == dfu._OP_ACTIVATE_AND_RESET


@pytest.mark.asyncio
async def test_dfu_upload_retries_transient_failure(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A dropped link mid-update is retried, like adafruit-nrfutil does."""
    monkeypatch.setattr(dfu, "_RETRY_DELAY", 0)
    package = _write_package(tmp_path)
    attempts = []

    async def _attempt(address: str, image: bytes, init_packet: bytes) -> None:
        attempts.append(address)
        if len(attempts) < 3:
            raise EsphomeError("DFU start failed with status 0x04")

    monkeypatch.setattr(dfu, "_dfu_attempt", _attempt)

    await dfu.dfu_upload("AA:BB:CC:DD:EE:FF", package)

    assert attempts == ["AA:BB:CC:DD:EE:FF"] * 3


@pytest.mark.asyncio
async def test_dfu_upload_gives_up_after_attempts(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setattr(dfu, "_RETRY_DELAY", 0)
    package = _write_package(tmp_path)
    calls = 0

    async def _attempt(address: str, image: bytes, init_packet: bytes) -> None:
        nonlocal calls
        calls += 1
        raise EsphomeError("DFU upload failed with status 0x04")

    monkeypatch.setattr(dfu, "_dfu_attempt", _attempt)

    with pytest.raises(EsphomeError, match="DFU upload failed"):
        await dfu.dfu_upload("AA:BB:CC:DD:EE:FF", package)

    assert calls == dfu._ATTEMPTS


@pytest.mark.asyncio
async def test_list_services_lowercases_uuids(monkeypatch: pytest.MonkeyPatch) -> None:
    class _Service:
        def __init__(self, uuid: str) -> None:
            self.uuid = uuid

    class _Client:
        def __init__(self) -> None:
            self.services = [_Service("8D53DC1D-1DB7-4CD3-868B-8A527460AA84")]

        async def __aenter__(self):
            return self

        async def __aexit__(self, *args) -> bool:
            return False

    monkeypatch.setattr(dfu, "BleakClient", lambda *args, **kwargs: _Client())

    assert await dfu.list_services("AA:BB:CC:DD:EE:FF") == {
        "8d53dc1d-1db7-4cd3-868b-8a527460aa84"
    }


@pytest.mark.asyncio
async def test_find_dfu_bootloader_prefers_triggered_device(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    class _Advertisement:
        def __init__(self, service_uuids: list[str]) -> None:
            self.service_uuids = service_uuids

    class _Device:
        def __init__(self, address: str) -> None:
            self.address = address

    devices = {
        "AA:BB:CC:DD:EE:FF": (
            _Device("AA:BB:CC:DD:EE:FF"),
            _Advertisement([dfu.DFU_SERVICE_UUID.upper()]),
        ),
        "11:22:33:44:55:66": (
            _Device("11:22:33:44:55:66"),
            _Advertisement([dfu.DFU_SERVICE_UUID]),
        ),
    }

    class _Scanner:
        @staticmethod
        async def discover(*args, **kwargs):
            return devices

    monkeypatch.setattr(dfu, "BleakScanner", _Scanner)

    assert await dfu.find_dfu_bootloader(["AA:BB:CC:DD:EE:FF"]) == "AA:BB:CC:DD:EE:FF"
