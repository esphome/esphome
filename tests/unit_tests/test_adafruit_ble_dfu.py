"""Tests for the Adafruit bootloader BLE DFU client."""

import asyncio
from collections.abc import Callable
import json
from pathlib import Path
from types import SimpleNamespace
from typing import Self
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
    assert dfu.load_firmware(_write_package(tmp_path)) == dfu.DfuPackage(
        "firmware.zip", b"\x01\x02", b"\x03"
    )


def test_load_firmware_without_manifest(tmp_path: Path) -> None:
    package = tmp_path / "firmware.zip"
    with zipfile.ZipFile(package, "w") as archive:
        archive.writestr("app.bin", b"\xaa")
        archive.writestr("app.dat", b"\xbb")
    assert dfu.load_firmware(package) == dfu.DfuPackage(
        "firmware.zip", b"\xaa", b"\xbb"
    )


def test_load_firmware_missing_file(tmp_path: Path) -> None:
    with pytest.raises(EsphomeError, match="not found"):
        dfu.load_firmware(tmp_path / "nope.zip")


def test_load_firmware_without_application(tmp_path: Path) -> None:
    package = tmp_path / "firmware.zip"
    with zipfile.ZipFile(package, "w") as archive:
        archive.writestr("manifest.json", json.dumps({"manifest": {"softdevice": {}}}))
    with pytest.raises(EsphomeError, match="not an application update package"):
        dfu.load_firmware(package)


@pytest.mark.parametrize(
    "content",
    [None, b"not a zip"],
    ids=["missing_image", "not_a_zip"],
)
def test_load_firmware_rejects_broken_package(
    tmp_path: Path, content: bytes | None
) -> None:
    package = tmp_path / "firmware.zip"
    if content is None:
        with zipfile.ZipFile(package, "w") as archive:
            archive.writestr(
                "manifest.json",
                json.dumps(
                    {
                        "manifest": {
                            "application": {"bin_file": "a.bin", "dat_file": "a.dat"}
                        }
                    }
                ),
            )
    else:
        package.write_bytes(content)
    with pytest.raises(EsphomeError, match="Invalid DFU package"):
        dfu.load_firmware(package)


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

    with pytest.raises(EsphomeError, match="DFU start .* rejected"):
        await dfu._perform_update(client, b"\x00" * 4, b"\x01")

    assert client.control_point_ops()[-1] == dfu._OP_RESET


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("response", "match"),
    [
        (None, "timed out waiting for the response to opcode 0x04"),
        ((dfu._OP_START_DFU, dfu._RESPONSE_SUCCESS), "expected 0x04"),
        (
            (dfu._OP_VALIDATE, 0x06),
            r"DFU validation \(opcode 0x04\) rejected .* status 0x06 \(operation failed\)",
        ),
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


class _LegacyBootloaderFake(_FakeBleClient):
    """Answers like the Nordic legacy bootloader while the image streams.

    A receipt follows every ``_PRN`` packets, except for the packet that
    completes the image, which only gets the RECEIVE_FIRMWARE_IMAGE response.
    """

    def __init__(self, image_size: int) -> None:
        super().__init__()
        self.image_size = image_size
        self.streaming = False
        self.packets = 0
        self.received = 0

    async def write_gatt_char(
        self, characteristic: str, data, response: bool = True
    ) -> None:
        data = bytes(data)
        if (
            characteristic == dfu.DFU_CONTROL_POINT_UUID
            and data[0] == dfu._OP_RECEIVE_FIRMWARE_IMAGE
        ):
            self.writes.append((characteristic, data, response))
            self.streaming = True
            return
        await super().write_gatt_char(characteristic, data, response)
        if characteristic != dfu.DFU_PACKET_UUID or not self.streaming:
            return
        self.packets += 1
        self.received += len(data)
        if self.received >= self.image_size:
            notification = [
                dfu._OP_RESPONSE_CODE,
                dfu._OP_RECEIVE_FIRMWARE_IMAGE,
                dfu._RESPONSE_SUCCESS,
            ]
        elif self.packets % dfu._PRN == 0:
            notification = [dfu._OP_PACKET_RECEIPT_NOTIF, 0, 0, 0, 0]
        else:
            return
        asyncio.get_running_loop().call_soon(
            self.handler, None, bytearray(notification)
        )


@pytest.mark.asyncio
@pytest.mark.parametrize("image_size", [140, 160, 170], ids=["7", "8", "9"])
async def test_perform_update_last_packet_gets_no_receipt(
    monkeypatch: pytest.MonkeyPatch, image_size: int
) -> None:
    """An image that ends on a receipt boundary is answered by the response only."""
    monkeypatch.setattr(dfu, "_START_DELAY", 0)
    monkeypatch.setattr(dfu, "_PRN", 8)
    monkeypatch.setattr(dfu, "_RECEIPT_TIMEOUT", 0.05)
    monkeypatch.setattr(dfu, "_LONG_RESPONSE_TIMEOUT", 0.05)
    client = _LegacyBootloaderFake(image_size)

    await dfu._perform_update(client, b"\x00" * image_size, b"\x01")

    # 20 byte chunks at MTU 23, so 160 bytes is exactly 8 packets
    assert client.packets == -(-image_size // 20)
    assert client.control_point_ops()[-2:] == [
        dfu._OP_VALIDATE,
        dfu._OP_ACTIVATE_AND_RESET,
    ]


class _RejectingBootloaderFake(_FakeBleClient):
    """Accepts start and init, then rejects the image after a few packets."""

    def __init__(self, reject_after: int) -> None:
        super().__init__()
        self.reject_after = reject_after
        self.streaming = False
        self.packets = 0

    async def write_gatt_char(
        self, characteristic: str, data, response: bool = True
    ) -> None:
        data = bytes(data)
        if (
            characteristic == dfu.DFU_CONTROL_POINT_UUID
            and data[0] == dfu._OP_RECEIVE_FIRMWARE_IMAGE
        ):
            self.writes.append((characteristic, data, response))
            self.streaming = True
            return
        await super().write_gatt_char(characteristic, data, response)
        if characteristic != dfu.DFU_PACKET_UUID or not self.streaming:
            return
        self.packets += 1
        if self.packets == self.reject_after:
            asyncio.get_running_loop().call_soon(
                self.handler,
                None,
                bytearray(
                    [dfu._OP_RESPONSE_CODE, dfu._OP_RECEIVE_FIRMWARE_IMAGE, 0x05]
                ),
            )


@pytest.mark.asyncio
async def test_perform_update_stops_on_upload_rejection(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A rejection between receipt boundaries ends the upload instead of timing out."""
    monkeypatch.setattr(dfu, "_START_DELAY", 0)
    monkeypatch.setattr(dfu, "_PRN", 8)
    monkeypatch.setattr(dfu, "_RECEIPT_TIMEOUT", 30)
    client = _RejectingBootloaderFake(reject_after=3)

    async with asyncio.timeout(5):
        with pytest.raises(
            dfu._DfuRejectedError, match=r"DFU upload \(opcode 0x03\) .*\(CRC error\)"
        ):
            await dfu._perform_update(client, b"\x00" * 400, b"\x01")

    # The rejection is seen at the next receipt boundary at the latest
    assert client.packets <= dfu._PRN
    assert dfu._OP_VALIDATE not in client.control_point_ops()


def test_notification_handler_ignores_empty_and_unknown() -> None:
    session = dfu._DfuSession()

    session.notification_handler(0, bytearray())
    session.notification_handler(0, bytearray([0x7F]))

    assert session.receipts == 0
    assert session._responses.empty()


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

    async def _find(
        address: str,
        request_error: Exception | None = None,
        known_dfu: set[str] | None = None,
    ) -> str:
        found.append(address)
        assert known_dfu is None
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

    with pytest.raises(
        EsphomeError, match=r"Other devices in DFU mode: 11:22:33:44:55:66\."
    ):
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
    package = dfu.load_firmware(_write_package(tmp_path))
    calls: list[str] = []

    async def _attempt(address: str, image: bytes, init_packet: bytes) -> None:
        calls.append(address)
        if len(calls) <= failures:
            raise EsphomeError("DFU failed: link lost")

    monkeypatch.setattr(dfu, "_dfu_attempt", _attempt)

    if failures < dfu._ATTEMPTS:
        await dfu.dfu_upload("AA:BB:CC:DD:EE:FF", package)
    else:
        with pytest.raises(EsphomeError, match="link lost"):
            await dfu.dfu_upload("AA:BB:CC:DD:EE:FF", package)

    assert calls == ["AA:BB:CC:DD:EE:FF"] * dfu._ATTEMPTS


@pytest.mark.asyncio
async def test_dfu_upload_does_not_retry_rejections(
    fast_dfu: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A status rejection from the bootloader is raised at once, not retried."""
    fast_dfu.setattr(dfu, "_RETRY_DELAY", 0)
    package = dfu.load_firmware(_write_package(tmp_path))

    class _Client(_FakeBleClient, _ConnectClient):
        def __init__(self) -> None:
            _FakeBleClient.__init__(self, status=0x05)
            _ConnectClient.__init__(self)
            self.sessions = 0

        async def start_notify(self, characteristic: str, handler) -> None:
            self.sessions += 1
            await super().start_notify(characteristic, handler)

    client = _Client()
    _use_client(fast_dfu, client)

    with pytest.raises(
        EsphomeError, match=r"DFU start \(opcode 0x01\) .* status 0x05 \(CRC error\)"
    ):
        await dfu.dfu_upload("AA:BB:CC:DD:EE:FF", package)

    assert client.sessions == 1


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

    with pytest.raises(
        EsphomeError, match=r"Other devices in DFU mode: 11:22:33:44:55:66\."
    ):
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

    with pytest.raises(EsphomeError, match="was not found") as err:
        await dfu.find_dfu_bootloader("AA:BB:CC:DD:EE:FE")
    assert str(err.value).endswith(dfu.BOOTLOADER_HINT)
    assert "DFU mode request" not in str(err.value)


@pytest.mark.asyncio
async def test_enter_dfu_mode_reports_request_error_if_bootloader_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A rejected request looks like a reboot, so it is reported only on failure."""
    error = dfu.BleakError("Protocol Error 0x05: Insufficient Authentication")
    _use_client(
        monkeypatch, _ConnectClient((dfu.TRIGGER_SERVICE_UUID,), write_error=error)
    )
    monkeypatch.setattr(dfu.BleakScanner, "find_device_by_filter", _advertising())

    with pytest.raises(
        EsphomeError,
        match="The DFU mode request failed: Protocol Error 0x05: Insufficient",
    ):
        await dfu.enter_dfu_mode("AA:BB:CC:DD:EE:FE")


MACOS_APP = "8D53DC1D-1DB7-4CD3-868B-8A527460AA84"
MACOS_BOOTLOADER = "0F9A6E21-5B3C-4D7E-9A10-2B3C4D5E6F70"
MACOS_STRANGER = "C1D2E3F4-A5B6-4C7D-8E9F-0A1B2C3D4E5F"


def _macos_scanner(
    monkeypatch: pytest.MonkeyPatch,
    before: list[str],
    after: list[str],
    app_keeps_advertising: bool = False,
) -> None:
    """DFU devices advertising before the request and after it."""
    dfu_adv = SimpleNamespace(service_uuids=[dfu.DFU_SERVICE_UUID.upper()])
    other_adv = SimpleNamespace(service_uuids=["0000180f-0000-1000-8000-00805f9b34fb"])

    class _Scanner:
        def __init__(self, detection_callback) -> None:
            self.callback = detection_callback

        @staticmethod
        async def discover(timeout: float, return_adv: bool):
            found = {MACOS_APP: (SimpleNamespace(address=MACOS_APP), other_adv)}
            for address in before:
                found[address] = (SimpleNamespace(address=address), dfu_adv)
            return found

        async def __aenter__(self) -> Self:
            self.callback(SimpleNamespace(address=MACOS_APP), other_adv)
            for address in after:
                self.callback(SimpleNamespace(address=address.lower()), dfu_adv)
            if app_keeps_advertising:
                self.callback(SimpleNamespace(address=MACOS_APP), other_adv)
            return self

        async def __aexit__(self, *exc_info) -> None:
            pass

    monkeypatch.setattr(dfu, "BleakScanner", _Scanner)
    monkeypatch.setattr(dfu, "_REBOOT_TIMEOUT", 0.01)
    monkeypatch.setattr(dfu, "_NEW_DEVICE_SETTLE", 0)
    _use_client(monkeypatch, _ConnectClient((dfu.TRIGGER_SERVICE_UUID,)))


@pytest.mark.asyncio
async def test_enter_dfu_mode_macos_accepts_the_one_new_dfu_device(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """MacOS gives the bootloader its own UUID; it is the DFU device that is new."""
    _macos_scanner(
        monkeypatch, before=[MACOS_STRANGER], after=[MACOS_STRANGER, MACOS_BOOTLOADER]
    )

    assert await dfu.enter_dfu_mode(MACOS_APP) == MACOS_BOOTLOADER.lower()


@pytest.mark.asyncio
async def test_enter_dfu_mode_macos_refuses_a_new_device_while_the_app_advertises(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A rejected trigger leaves the app running; another board entering DFU mode is not its bootloader."""
    _macos_scanner(
        monkeypatch, before=[], after=[MACOS_STRANGER], app_keeps_advertising=True
    )

    with pytest.raises(EsphomeError, match="Other devices in DFU mode: "):
        await dfu.enter_dfu_mode(MACOS_APP)


@pytest.mark.asyncio
async def test_enter_dfu_mode_macos_refuses_two_new_dfu_devices(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _macos_scanner(monkeypatch, before=[], after=[MACOS_BOOTLOADER, MACOS_STRANGER])

    with pytest.raises(EsphomeError, match="Other devices in DFU mode: ") as err:
        await dfu.enter_dfu_mode(MACOS_APP)
    assert MACOS_BOOTLOADER.lower() in str(err.value)
    assert MACOS_STRANGER.lower() in str(err.value)


@pytest.mark.asyncio
async def test_enter_dfu_mode_macos_refuses_a_dfu_device_seen_before(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A device that was in DFU mode before the request is not the bootloader."""
    _macos_scanner(monkeypatch, before=[MACOS_STRANGER], after=[MACOS_STRANGER])

    with pytest.raises(
        EsphomeError, match=f"Other devices in DFU mode: {MACOS_STRANGER}\\."
    ):
        await dfu.enter_dfu_mode(MACOS_APP)
