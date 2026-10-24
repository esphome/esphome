"""Tests for the Adafruit bootloader BLE DFU client."""

import asyncio
from collections.abc import Callable
import json
from pathlib import Path
from types import SimpleNamespace
import zipfile

from bleak.exc import BleakDBusError
import pytest

from esphome.components.adafruit_ble import dfu
from esphome.core import EsphomeError

WINDOWS_CANCELLED = OSError(-2147023673, "The operation was canceled by the user.")


@pytest.fixture
def fast_dfu(monkeypatch: pytest.MonkeyPatch) -> pytest.MonkeyPatch:
    """No start delay and no packet receipts unless a test asks for them."""
    monkeypatch.setattr(dfu, "_START_DELAY", 0)
    monkeypatch.setattr(dfu, "_PRN", 1000)
    return monkeypatch


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
    """Also reports a packet receipt notification for every ``_PRN`` packets."""

    def __init__(self, early: bool = False) -> None:
        super().__init__()
        self.packets = 0
        # Deliver the receipt inside the write, before the stream starts waiting
        self.early = early

    async def write_gatt_char(
        self, characteristic: str, data, response: bool = True
    ) -> None:
        await super().write_gatt_char(characteristic, data, response)
        if characteristic != dfu.DFU_PACKET_UUID:
            return
        self.packets += 1
        if self.packets % dfu._PRN:
            return
        receipt = bytearray([dfu._OP_PACKET_RECEIPT_NOTIF, 0, 0, 0, 0])
        if self.early:
            self.handler(None, receipt)
        else:
            asyncio.get_running_loop().call_soon(self.handler, None, receipt)


class _ConnectClient:
    """Connects, exposes ``services`` and records writes."""

    def __init__(
        self,
        services: tuple[str, ...] = (),
        connect_error: Exception | None = None,
        write_error: Exception | None = None,
    ) -> None:
        self.services = [SimpleNamespace(uuid=uuid.upper()) for uuid in services]
        self.connect_error = connect_error
        self.write_error = write_error
        self.writes: list[tuple[str, bytes]] = []
        self.disconnected = False

    async def connect(self) -> None:
        if self.connect_error is not None:
            raise self.connect_error

    async def disconnect(self) -> None:
        self.disconnected = True

    async def write_gatt_char(self, characteristic, data, response=True) -> None:
        self.writes.append((characteristic, bytes(data)))
        if self.write_error is not None:
            raise self.write_error


def _use_client(monkeypatch: pytest.MonkeyPatch, client: _ConnectClient) -> None:
    monkeypatch.setattr(dfu, "BleakClient", lambda *args, **kwargs: client)


def _not_permitted() -> BleakDBusError:
    return BleakDBusError("org.bluez.Error.NotPermitted", [])


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
    assert dfu._load_firmware(_write_package(tmp_path)) == (b"\x01\x02", b"\x03")


def test_load_firmware_without_manifest(tmp_path: Path) -> None:
    package = tmp_path / "firmware.zip"
    with zipfile.ZipFile(package, "w") as archive:
        archive.writestr("app.bin", b"\xaa")
        archive.writestr("app.dat", b"\xbb")
    assert dfu._load_firmware(package) == (b"\xaa", b"\xbb")


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
async def test_perform_update_opcode_sequence(fast_dfu: pytest.MonkeyPatch) -> None:
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
async def test_perform_update_reports_failure(fast_dfu: pytest.MonkeyPatch) -> None:
    client = _FakeBleClient(status=0x04)

    with pytest.raises(EsphomeError, match="DFU start failed"):
        await dfu._perform_update(client, b"\x00" * 4, b"\x01")

    assert client.control_point_ops()[-1] == dfu._OP_RESET


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("response", "match"),
    [
        (None, "timed out waiting for the response to opcode 0x04"),
        ((dfu._OP_START_DFU, dfu._RESPONSE_SUCCESS), "expected 0x04"),
        ((dfu._OP_VALIDATE, 0x06), "DFU validation failed with status 0x06"),
    ],
)
async def test_expect_success_failures(
    response: tuple[int, int] | None, match: str
) -> None:
    session = dfu._DfuSession()
    if response is not None:
        session.notification_handler(0, bytearray([dfu._OP_RESPONSE_CODE, *response]))

    with pytest.raises(EsphomeError, match=match):
        await session.expect_success(dfu._OP_VALIDATE, "validation", timeout=0.01)


@pytest.mark.asyncio
@pytest.mark.parametrize("early", [False, True])
async def test_stream_firmware_waits_for_packet_receipt(
    monkeypatch: pytest.MonkeyPatch, early: bool
) -> None:
    """A receipt that arrives before the stream waits for it is not lost."""
    monkeypatch.setattr(dfu, "_PRN", 2)
    client = _ReceiptFakeBleClient(early=early)
    session = dfu._DfuSession()
    client.handler = session.notification_handler

    await dfu._stream_firmware(client, session, b"\x00" * 100)

    assert len(client.packet_writes()) == 5
    assert session.receipts == 2


@pytest.mark.asyncio
async def test_stream_firmware_raises_on_missed_receipt(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(dfu, "_PRN", 2)
    monkeypatch.setattr(dfu, "_RECEIPT_TIMEOUT", 0.01)
    client = _FakeBleClient()
    session = dfu._DfuSession()
    client.handler = session.notification_handler

    with pytest.raises(EsphomeError, match="packet receipt 1"):
        await dfu._stream_firmware(client, session, b"\x00" * 100)

    assert len(client.packet_writes()) == 2


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "write_error", [None, dfu.BleakError("disconnected"), WINDOWS_CANCELLED]
)
async def test_enter_dfu_mode_triggers_reboot(
    monkeypatch: pytest.MonkeyPatch, write_error: Exception | None
) -> None:
    """The reboot may drop the link, so the write can fail instead of being acked."""
    client = _ConnectClient((dfu.TRIGGER_SERVICE_UUID,), write_error=write_error)
    _use_client(monkeypatch, client)
    found: list[str] = []

    async def _find(address: str) -> str:
        found.append(address)
        return "AA:BB:CC:DD:EE:00"

    monkeypatch.setattr(dfu, "find_dfu_bootloader", _find)

    assert await dfu.enter_dfu_mode("AA:BB:CC:DD:EE:FF") == "AA:BB:CC:DD:EE:00"
    assert client.writes == [(dfu.TRIGGER_CHARACTERISTIC_UUID, dfu.TRIGGER_MAGIC)]
    assert client.disconnected
    assert found == ["AA:BB:CC:DD:EE:FF"]


@pytest.mark.asyncio
async def test_enter_dfu_mode_already_in_bootloader(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client = _ConnectClient((dfu.DFU_SERVICE_UUID,))
    _use_client(monkeypatch, client)

    assert await dfu.enter_dfu_mode("AA:BB:CC:DD:EE:FF") == "AA:BB:CC:DD:EE:FF"
    assert client.writes == []


@pytest.mark.asyncio
async def test_enter_dfu_mode_without_ota_service(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _use_client(monkeypatch, _ConnectClient(("8d53dc1d-1db7-4cd3-868b-8a527460aa84",)))

    with pytest.raises(EsphomeError, match="does not offer an OTA service"):
        await dfu.enter_dfu_mode("AA:BB:CC:DD:EE:FF")


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "client",
    [
        _ConnectClient(connect_error=_not_permitted()),
        _ConnectClient((dfu.TRIGGER_SERVICE_UUID,), write_error=_not_permitted()),
    ],
    ids=["connect", "write"],
)
async def test_enter_dfu_mode_pairing_hint(
    monkeypatch: pytest.MonkeyPatch, client: _ConnectClient
) -> None:
    _use_client(monkeypatch, client)

    with pytest.raises(EsphomeError, match="Make sure the device is paired"):
        await dfu.enter_dfu_mode("AA:BB:CC:DD:EE:FF")


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("error", "match"),
    [
        (dfu.BleakDeviceNotFoundError("AA:BB:CC:DD:EE:FF"), "was not found"),
        (dfu.BleakError("boom"), "Could not connect"),
        (WINDOWS_CANCELLED, "Could not connect"),
    ],
)
async def test_connect_maps_errors(
    monkeypatch: pytest.MonkeyPatch, error: Exception, match: str
) -> None:
    _use_client(monkeypatch, _ConnectClient(connect_error=error))

    with pytest.raises(EsphomeError, match=match):
        async with dfu._connect("AA:BB:CC:DD:EE:FF"):
            pass


@pytest.mark.asyncio
async def test_enter_dfu_mode_finds_device_left_in_bootloader(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A failed upload can leave the device in its bootloader at the next address."""
    _use_client(
        monkeypatch,
        _ConnectClient(connect_error=dfu.BleakDeviceNotFoundError("AA:BB:CC:DD:EE:FE")),
    )
    monkeypatch.setattr(
        dfu.BleakScanner,
        "find_device_by_filter",
        _advertising(
            ("11:22:33:44:55:66", [dfu.DFU_SERVICE_UUID]),
            ("AA:BB:CC:DD:EE:FF", [dfu.DFU_SERVICE_UUID]),
        ),
    )

    assert await dfu.enter_dfu_mode("AA:BB:CC:DD:EE:FE") == "AA:BB:CC:DD:EE:FF"


@pytest.mark.asyncio
async def test_enter_dfu_mode_not_found_anywhere(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _use_client(
        monkeypatch,
        _ConnectClient(connect_error=dfu.BleakDeviceNotFoundError("AA:BB:CC:DD:EE:FE")),
    )
    monkeypatch.setattr(
        dfu.BleakScanner,
        "find_device_by_filter",
        _advertising(("11:22:33:44:55:66", [dfu.DFU_SERVICE_UUID])),
    )

    with pytest.raises(EsphomeError, match="11:22:33:44:55:66. Pass the right one"):
        await dfu.enter_dfu_mode("AA:BB:CC:DD:EE:FE")


@pytest.mark.asyncio
async def test_perform_update_tolerates_dropped_link_on_activate(
    fast_dfu: pytest.MonkeyPatch,
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
                raise WINDOWS_CANCELLED
            await super().write_gatt_char(characteristic, data, response)

    client = _Client()

    await dfu._perform_update(client, b"\x00" * 40, b"\x01")

    assert client.control_point_ops()[-1] == dfu._OP_ACTIVATE_AND_RESET


@pytest.mark.asyncio
@pytest.mark.parametrize("failures", [2, 3], ids=["recovers", "gives_up"])
async def test_dfu_upload_retries(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path, failures: int
) -> None:
    """A dropped link mid-update is retried, like adafruit-nrfutil does."""
    monkeypatch.setattr(dfu, "_RETRY_DELAY", 0)
    package = _write_package(tmp_path)
    calls: list[str] = []

    async def _attempt(address: str, image: bytes, init_packet: bytes) -> None:
        calls.append(address)
        if len(calls) <= failures:
            raise EsphomeError("DFU upload failed with status 0x04")

    monkeypatch.setattr(dfu, "_dfu_attempt", _attempt)

    if failures < dfu._ATTEMPTS:
        await dfu.dfu_upload("AA:BB:CC:DD:EE:FF", package)
    else:
        with pytest.raises(EsphomeError, match="DFU upload failed"):
            await dfu.dfu_upload("AA:BB:CC:DD:EE:FF", package)

    assert calls == ["AA:BB:CC:DD:EE:FF"] * dfu._ATTEMPTS


@pytest.mark.asyncio
async def test_dfu_attempt_maps_link_errors(
    fast_dfu: pytest.MonkeyPatch,
) -> None:
    class _Client(_FakeBleClient, _ConnectClient):
        def __init__(self) -> None:
            _FakeBleClient.__init__(self)
            _ConnectClient.__init__(self)

        async def start_notify(self, characteristic: str, handler) -> None:
            raise dfu.BleakError("link lost")

    client = _Client()
    _use_client(fast_dfu, client)

    with pytest.raises(EsphomeError, match="DFU failed: link lost"):
        await dfu._dfu_attempt("AA:BB:CC:DD:EE:FF", b"\x00", b"\x01")
    assert client.disconnected


def _advertising(
    *devices: tuple[str, list[str]],
) -> Callable[..., object]:
    """A find_device_by_filter that offers ``devices`` to the filter in order."""

    async def _find(filterfunc, timeout: float):
        for address, uuids in devices:
            device = SimpleNamespace(address=address)
            if filterfunc(device, SimpleNamespace(service_uuids=uuids)):
                return device
        return None

    return _find


@pytest.mark.asyncio
@pytest.mark.parametrize("bootloader", ["AA:BB:CC:DD:EE:FE", "aa:bb:cc:dd:ee:ff"])
async def test_find_dfu_bootloader_accepts_triggered_device(
    monkeypatch: pytest.MonkeyPatch, bootloader: str
) -> None:
    """The bootloader uses the application address or the one above it."""
    monkeypatch.setattr(
        dfu.BleakScanner,
        "find_device_by_filter",
        _advertising(
            ("11:22:33:44:55:66", [dfu.DFU_SERVICE_UUID]),
            ("AA:BB:CC:DD:EE:FE", ["0000180f-0000-1000-8000-00805f9b34fb"]),
            (bootloader, [dfu.DFU_SERVICE_UUID.upper()]),
        ),
    )

    assert await dfu.find_dfu_bootloader("AA:BB:CC:DD:EE:FE") == bootloader


@pytest.mark.asyncio
async def test_find_dfu_bootloader_never_picks_a_stranger(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Another device in DFU mode is listed, not flashed."""
    monkeypatch.setattr(
        dfu.BleakScanner,
        "find_device_by_filter",
        _advertising(("11:22:33:44:55:66", [dfu.DFU_SERVICE_UUID])),
    )

    with pytest.raises(EsphomeError, match="11:22:33:44:55:66. Pass the right one"):
        await dfu.find_dfu_bootloader("AA:BB:CC:DD:EE:FE")


@pytest.mark.asyncio
async def test_find_dfu_bootloader_by_platform_uuid(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """MacOS names devices by UUID, so only the same identifier is accepted."""
    uuid = "8D53DC1D-1DB7-4CD3-868B-8A527460AA84"
    monkeypatch.setattr(
        dfu.BleakScanner,
        "find_device_by_filter",
        _advertising((uuid, [dfu.DFU_SERVICE_UUID])),
    )

    assert await dfu.find_dfu_bootloader(uuid) == uuid


@pytest.mark.asyncio
async def test_find_dfu_bootloader_nothing_found(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(dfu.BleakScanner, "find_device_by_filter", _advertising())

    with pytest.raises(EsphomeError, match="was not found.$"):
        await dfu.find_dfu_bootloader("AA:BB:CC:DD:EE:FE")
