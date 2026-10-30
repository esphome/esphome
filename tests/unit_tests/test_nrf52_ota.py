"""Tests for nRF52 BLE device discovery and OTA transport selection."""

import asyncio
import logging
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest
from smp.image_management import (
    ImageStatesReadResponse,
    ImageUploadWriteRequest,
    ImageUploadWriteResponse,
)
from smpclient.requests.image_management import ImageStatesRead

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
    ("device", "udp", "transport"),
    [
        ("/dev/ttyACM0", False, "serial"),
        ("COM3", False, "serial"),
        (MAC, False, "ble"),
        (UUID, False, "ble"),
        ("fd00::1", True, "udp"),
    ],
)
def test_upload_transport(monkeypatch, device: str, udp: bool, transport: str) -> None:
    """The caller picks UDP; otherwise MAC addresses and macOS UUIDs go to BLE, the rest to serial."""
    captured: dict = {}

    class FakeClient:
        def __init__(self, transport, address, timeout_s: float = 2.5):
            captured["transport"] = transport
            captured["address"] = address
            captured["timeout_s"] = timeout_s

        async def connect(self) -> None:
            pass

        async def disconnect(self) -> None:
            pass

    monkeypatch.setattr(ota, "_get_image_tlv_sha256", lambda firmware: b"")
    monkeypatch.setattr(ota, "_smpmgr_upload_connected", AsyncMock())
    monkeypatch.setattr(ota, "SMPSerialTransport", lambda: "serial")
    monkeypatch.setattr(ota, "SMPBLETransport", lambda: "ble")
    monkeypatch.setattr(ota, "_SequenceMatchingUDPTransport", lambda mtu: "udp")
    monkeypatch.setattr(ota, "SMPClient", FakeClient)

    asyncio.run(ota._smpmgr_upload(device, Path("firmware.bin"), udp))  # pylint: disable=protected-access
    assert captured == {
        "transport": transport,
        "address": device,
        "timeout_s": ota.SMP_UDP_PROBE_TIMEOUT if udp else 2.5,
    }


IMAGE = bytes(range(256)) * 20


class FakeUploadClient:
    """Answers image writes like mcumgr: a write at the wrong offset gets the expected one."""

    def __init__(self, replies: list) -> None:
        # Each entry is an exception to raise, an offset to report, or None to accept
        self.replies = replies
        self.device_off = 0
        self.offsets: list[int] = []

    def get_max_cbor_and_data_size(self, request) -> tuple[int, int]:
        return 0, 1000

    async def request(self, request, timeout_s=None):
        self.offsets.append(request.off)
        reply = self.replies.pop(0) if self.replies else None
        if isinstance(reply, BaseException):
            raise reply
        if reply is not None:
            self.device_off = reply
        elif request.off == self.device_off:
            self.device_off += len(request.data)
        done = self.device_off == len(IMAGE)
        return SimpleNamespace(off=self.device_off, match=True if done else None)


PROGRESS = SimpleNamespace(update=lambda _: None)


def _udp_upload(client: FakeUploadClient) -> None:
    asyncio.run(ota._udp_upload(client, IMAGE, PROGRESS, "fd00::1", b"hash"))  # pylint: disable=protected-access


def test_udp_upload_resends_lost_replies(monkeypatch) -> None:
    monkeypatch.setattr(ota, "error", lambda response: False)
    client = FakeUploadClient([TimeoutError(), None, TimeoutError(), None])
    _udp_upload(client)
    assert client.offsets == [0, 0, 1000, 1000, 2000, 3000, 4000, 5000]


def test_udp_upload_follows_the_device_offset(monkeypatch) -> None:
    """A device that restarted the upload is sent the image again from its offset."""
    monkeypatch.setattr(ota, "error", lambda response: False)
    client = FakeUploadClient([None, None, 0])
    _udp_upload(client)
    assert client.offsets[:4] == [0, 1000, 2000, 0]
    assert client.device_off == len(IMAGE)


def test_udp_upload_gives_up_without_progress(monkeypatch) -> None:
    monkeypatch.setattr(ota, "error", lambda response: False)
    client = FakeUploadClient([0] * (ota.SMP_UDP_UPLOAD_RETRIES + 1))
    with pytest.raises(EsphomeError, match="not advancing past offset 0"):
        _udp_upload(client)


def test_udp_upload_gives_up_after_retries(monkeypatch) -> None:
    monkeypatch.setattr(ota, "error", lambda response: False)
    client = FakeUploadClient([TimeoutError()] * (ota.SMP_UDP_UPLOAD_RETRIES + 1))
    with pytest.raises(EsphomeError, match="stalled at offset 0"):
        _udp_upload(client)
    assert len(client.offsets) == ota.SMP_UDP_UPLOAD_RETRIES + 1


def test_udp_upload_reports_a_hash_mismatch(monkeypatch) -> None:
    monkeypatch.setattr(ota, "error", lambda response: False)
    client = FakeUploadClient([])

    async def request(request, timeout_s=None):
        return SimpleNamespace(off=len(IMAGE), match=False)

    client.request = request
    with pytest.raises(EsphomeError, match="mismatched SHA256"):
        _udp_upload(client)


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


def test_udp_upload_gives_up_on_repeated_restarts(monkeypatch) -> None:
    """A device that keeps going back to 0 never sets a new high, so the upload stops."""
    monkeypatch.setattr(ota, "error", lambda response: False)
    client = FakeUploadClient([None, 0] * (ota.SMP_UDP_UPLOAD_RETRIES + 1))
    with pytest.raises(EsphomeError, match="not advancing past offset 1000"):
        _udp_upload(client)


def test_udp_upload_rejects_an_offset_past_the_image(monkeypatch) -> None:
    monkeypatch.setattr(ota, "error", lambda response: False)
    client = FakeUploadClient([len(IMAGE) + 1])
    with pytest.raises(EsphomeError, match="past"):
        _udp_upload(client)


def test_udp_request_failure_names_the_target() -> None:
    client = SimpleNamespace(request=AsyncMock(side_effect=TimeoutError()))
    with pytest.raises(
        EsphomeError, match="fd00::1 on UDP port 1337 could not be reached"
    ):
        asyncio.run(
            ota._request_with_retry(  # pylint: disable=protected-access
                client, ImageStatesRead, None, 2, "fd00::1 on UDP port 1337"
            )
        )
    assert client.request.await_count == 3


def test_udp_upload_skips_a_late_reply(monkeypatch) -> None:
    """A reply to a timed-out attempt is still queued when the resend goes out.

    The plain transport would read it as the answer to the resend and stay one
    reply behind for the rest of the upload.
    """

    monkeypatch.setattr(ota, "SMP_UDP_REQUEST_TIMEOUT", 0.05)

    async def run() -> int:
        transport = ota._SequenceMatchingUDPTransport(mtu=1280)  # pylint: disable=protected-access
        queue: asyncio.Queue[bytes] = asyncio.Queue()
        state = {"off": 0, "sent": 0, "late": None}

        async def send(data: bytes) -> None:
            state["sent"] += 1
            request = ImageUploadWriteRequest.loads(data)
            if request.off == state["off"]:
                state["off"] += len(request.data)
            reply = bytes(
                ImageUploadWriteResponse(
                    sequence=request.header.sequence,
                    off=state["off"],
                    match=True if state["off"] == len(IMAGE) else None,
                )
            )
            if state["sent"] == 1:
                state["late"] = reply  # lost until the resend goes out
                return
            if state["late"] is not None:
                queue.put_nowait(state["late"])
                state["late"] = None
            queue.put_nowait(reply)

        transport.send = send
        transport.receive = queue.get
        client = ota.SMPClient(transport, "fd00::1", timeout_s=0.05)
        await ota._udp_upload(client, IMAGE, PROGRESS, "fd00::1", b"hash")  # pylint: disable=protected-access
        return state["sent"]

    sent = asyncio.run(run())
    # One resend for the lost reply, then one request per chunk
    assert sent == len(range(0, len(IMAGE), 1252)) + 1


def test_udp_no_reply_to_the_first_request_is_unreachable(monkeypatch) -> None:
    """The caller tries the next address when the device never answers."""
    client = SimpleNamespace(request=AsyncMock(side_effect=TimeoutError()))
    with pytest.raises(ota.SMPUnreachableError, match="could not be reached"):
        asyncio.run(
            ota._smpmgr_upload_connected(  # pylint: disable=protected-access
                client, "fd00::5", Path("firmware.bin"), b"", True
            )
        )


class ResetDevice:
    """Plays out each ResetWrite: "ok", "lost" (never arrives) or "applied" (reply lost).

    With swaps=False the device reboots into the old image, as MCUboot does with an
    image too large for its slot.
    """

    def __init__(
        self,
        outcomes: list[str],
        answers: bool = True,
        ours: bool = True,
        swaps: bool = True,
        silent: int = 0,
    ) -> None:
        self.outcomes = outcomes
        self.answers = answers
        self.silent = silent
        self.ours = ours
        self.swaps = swaps
        self.new_image = False
        self.resets = 0

    async def request(self, request, timeout_s=None):
        if isinstance(request, ota.ResetWrite):
            self.resets += 1
            outcome = self.outcomes.pop(0)
            if outcome == "ok":
                self.new_image = self.swaps
                return SimpleNamespace()
            self.new_image = self.new_image or outcome == "applied"
            raise TimeoutError
        if not self.answers or self.silent:
            self.silent = max(self.silent - 1, 0)
            raise TimeoutError
        image = SimpleNamespace(
            hash=b"hash" if self.ours else b"other",
            active=self.new_image,
            pending=not self.new_image,
        )
        return SimpleNamespace(images=[image])


@pytest.mark.parametrize(
    ("device", "resets", "result"),
    [
        # The reset took effect and only its reply was lost: never reset the new image
        (ResetDevice(["applied"]), 1, "ok"),
        # The reset never arrived and the old image still runs: resend once
        (ResetDevice(["lost", "ok"]), 2, "ok"),
        (ResetDevice(["lost", "lost"]), 2, "still runs the old image"),
        (ResetDevice(["lost"], ours=False), 1, "no longer has the new image"),
        # No answer at all: assume it is rebooting
        (ResetDevice(["lost"], answers=False), 1, "warning"),
        # A confirmed reset is followed until the new image runs
        (ResetDevice(["ok"]), 1, "ok"),
        (ResetDevice(["ok"], swaps=False), 1, "did not swap in the new image"),
        (ResetDevice(["ok"], answers=False), 1, "warning"),
        # Replies lost and the image too large: the old image is reset once more
        (
            ResetDevice(["lost", "ok"], silent=1, swaps=False),
            2,
            "did not swap in the new image",
        ),
    ],
)
def test_udp_reset(
    monkeypatch, caplog, device: ResetDevice, resets: int, result: str
) -> None:
    monkeypatch.setattr(ota, "error", lambda response: False)
    monkeypatch.setattr(ota, "success", lambda response: True)
    monkeypatch.setattr(ota, "SMP_UDP_PROBE_TIMEOUT", 0)
    monkeypatch.setattr(ota, "SMP_UDP_REBOOT_WAIT", 0.05)
    monkeypatch.setattr(ota, "SMP_UDP_LOST_RESET_WAIT", 0.05)
    run = ota._udp_reset(device, "fd00::5", b"hash")  # pylint: disable=protected-access
    if result in ("ok", "warning"):
        asyncio.run(run)
    else:
        with pytest.raises(EsphomeError, match=result):
            asyncio.run(run)
    assert device.resets == resets
    warned = any(
        r.levelname == "WARNING" and "check that it runs the new image" in r.message
        for r in caplog.records
    )
    assert warned is (result == "warning")


def test_udp_reset_probe_error_reply(monkeypatch) -> None:
    """An error reply to the probe is a failed read, not a missing image."""
    monkeypatch.setattr(ota, "error", lambda response: False)
    monkeypatch.setattr(ota, "success", lambda response: False)
    monkeypatch.setattr(ota, "SMP_UDP_PROBE_TIMEOUT", 0)
    monkeypatch.setattr(ota, "SMP_UDP_LOST_RESET_WAIT", 0.05)
    device = ResetDevice(["lost"])
    with pytest.raises(EsphomeError, match="answered with an error"):
        asyncio.run(ota._udp_reset(device, "fd00::5", b"hash"))  # pylint: disable=protected-access
    assert device.resets == 1


class ClearingDevice:
    """Clears its upload state after the last write, like Zephyr's img_mgmt."""

    def __init__(self) -> None:
        self.off = 0
        self.stored = False
        self.lose_final_reply = True
        self.writes: list[int] = []

    def get_max_cbor_and_data_size(self, request) -> tuple[int, int]:
        return 0, 1000

    async def request(self, request, timeout_s=None):
        if isinstance(request, ImageStatesRead):
            stored = b"hash" if self.stored else b"old"
            return SimpleNamespace(images=[SimpleNamespace(hash=stored)])
        self.writes.append(request.off)
        if request.off == self.off:
            self.off += len(request.data)
            if self.off == len(IMAGE):
                self.stored = True
                self.off = 0
                if self.lose_final_reply:
                    self.lose_final_reply = False
                    raise TimeoutError
                return SimpleNamespace(off=len(IMAGE), match=True)
        return SimpleNamespace(off=self.off, match=None)


def test_udp_upload_lost_final_reply_does_not_restart(monkeypatch) -> None:
    """The resent final chunk gets offset 0; the image is in the slot, so it is done."""
    monkeypatch.setattr(ota, "error", lambda response: False)
    monkeypatch.setattr(ota, "success", lambda response: True)
    device = ClearingDevice()
    asyncio.run(ota._udp_upload(device, IMAGE, PROGRESS, "fd00::1", b"hash"))  # pylint: disable=protected-access
    assert device.writes == [0, 1000, 2000, 3000, 4000, 5000, 5000]


def test_image_in_slot_logs_why_it_sends_again(monkeypatch, caplog) -> None:
    client = SimpleNamespace(request=AsyncMock(return_value="rc=8"))
    monkeypatch.setattr(ota, "success", lambda response: False)
    assert not asyncio.run(ota._image_in_slot(client, b"hash", "fd00::5"))  # pylint: disable=protected-access
    assert "Could not read the image states" in caplog.text
    assert "rc=8" in caplog.text


def _timed_out_then_answered_client() -> ota.SMPClient:
    """A real SMPClient whose first ImageStatesRead goes unanswered."""
    transport = ota._SequenceMatchingUDPTransport(mtu=1280)  # pylint: disable=protected-access
    queue: asyncio.Queue[bytes] = asyncio.Queue()
    sent = []

    async def send(data: bytes) -> None:
        sent.append(data)
        if len(sent) > 1:
            sequence = data[6]
            queue.put_nowait(
                bytes(ImageStatesReadResponse(sequence=sequence, images=[]))
            )

    transport.send = send
    transport.receive = queue.get
    return ota.SMPClient(transport, "fd00::1", timeout_s=0.02)


@pytest.mark.parametrize("udp", [True, False])
def test_udp_upload_hides_smpclient_timeout_errors(
    monkeypatch, caplog, udp: bool
) -> None:
    """A recovered timeout logs no smpclient ERROR on UDP; BLE and serial keep it."""

    async def upload(device: str, firmware: Path, udp: bool) -> None:
        client = _timed_out_then_answered_client()
        await ota._request_with_retry(client, ImageStatesRead, None, 1, device)  # pylint: disable=protected-access

    monkeypatch.setattr(ota, "_smpmgr_upload", upload)
    asyncio.run(ota.smpmgr_upload("fd00::1", Path("firmware.bin"), udp))
    errors = [
        r
        for r in caplog.records
        if r.name == "smpclient" and r.levelno >= logging.ERROR
    ]
    assert bool(errors) is not udp
    assert not logging.getLogger("smpclient").filters


def test_udp_retry_exhaustion_still_raises(monkeypatch) -> None:
    async def upload(device: str, firmware: Path, udp: bool) -> None:
        client = _timed_out_then_answered_client()
        await ota._request_with_retry(client, ImageStatesRead, None, 0, device)  # pylint: disable=protected-access

    monkeypatch.setattr(ota, "_smpmgr_upload", upload)
    with pytest.raises(EsphomeError, match="could not be reached"):
        asyncio.run(ota.smpmgr_upload("fd00::1", Path("firmware.bin"), True))


def _state_client(*images: SimpleNamespace) -> SimpleNamespace:
    return SimpleNamespace(
        request=AsyncMock(return_value=SimpleNamespace(images=list(images)))
    )


def _slot(hash_: bytes, active: bool, pending: bool) -> SimpleNamespace:
    return SimpleNamespace(
        hash=hash_, active=active, pending=pending, confirmed=active, slot=0
    )


@pytest.mark.parametrize("udp", [True, False])
def test_upload_refuses_a_stale_pending_image(monkeypatch, udp: bool) -> None:
    """An earlier update MCUboot never swapped in blocks the slot on every transport."""
    monkeypatch.setattr(ota, "error", lambda response: False)
    monkeypatch.setattr(ota, "success", lambda response: True)
    client = _state_client(
        _slot(b"running", active=True, pending=False),
        _slot(b"earlier", active=False, pending=True),
    )
    with pytest.raises(EsphomeError, match="earlier update pending.*over USB"):
        asyncio.run(
            ota._smpmgr_upload_connected(  # pylint: disable=protected-access
                client, "fd00::5", Path("firmware.bin"), b"new", udp
            )
        )
    assert client.request.await_count == 1


def test_upload_keeps_the_same_pending_image(monkeypatch) -> None:
    """Our own pending image is marked and reset again, not refused as stale."""
    monkeypatch.setattr(ota, "error", lambda response: False)
    monkeypatch.setattr(ota, "success", lambda response: True)
    monkeypatch.setattr(ota, "RESET_DELAY", 0)
    client = _state_client(
        _slot(b"running", active=True, pending=False),
        _slot(b"new", active=False, pending=True),
    )
    asyncio.run(
        ota._smpmgr_upload_connected(  # pylint: disable=protected-access
            client, "/dev/ttyACM0", Path("firmware.bin"), b"new", False
        )
    )
    # Image states, mark for test, reset; no upload
    assert client.request.await_count == 3
