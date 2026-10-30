"""Tests for nRF52 BLE device discovery and OTA transport selection."""

import asyncio
from collections.abc import Callable
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

from esphome.components.nrf52 import ble_logger, ota, ota_udp, show_logs
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
    monkeypatch.setattr(ota_udp, "_SequenceMatchingUDPTransport", lambda mtu: "udp")
    monkeypatch.setattr(ota, "SMPClient", FakeClient)

    asyncio.run(ota._smpmgr_upload(device, Path("firmware.bin"), udp))  # pylint: disable=protected-access
    assert captured == {
        "transport": transport,
        "address": device,
        "timeout_s": ota_udp.SMP_UDP_PROBE_TIMEOUT if udp else 2.5,
    }


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


@pytest.fixture
def smp_ok(monkeypatch: pytest.MonkeyPatch) -> None:
    """Take every fake reply as a successful SMP response."""
    for module in (ota, ota_udp):
        monkeypatch.setattr(module, "error", lambda response: False)
        monkeypatch.setattr(module, "success", lambda response: True)


def _queue_client(
    reply: Callable[[bytes], list[bytes]], timeout_s: float = 0.05
) -> ota.SMPClient:
    """A real SMPClient over the UDP transport, fed by reply(request) frames."""
    transport = ota_udp._SequenceMatchingUDPTransport(mtu=1280)  # pylint: disable=protected-access
    queue: asyncio.Queue[bytes] = asyncio.Queue()

    async def send(data: bytes) -> None:
        for frame in reply(data):
            queue.put_nowait(frame)

    transport.send = send
    transport.receive = queue.get
    return ota.SMPClient(transport, "fd00::1", timeout_s=timeout_s)


IMAGE = bytes(range(256)) * 20
PROGRESS = SimpleNamespace(update=lambda _: None)


class FakeDevice:
    """Answers image writes and state reads like mcumgr.

    A write at the wrong offset gets the offset the device wants. Each scripted
    reply is an exception to raise, an offset to report, or None to accept. Like
    Zephyr's img_mgmt, the device clears its upload state after the last write.
    """

    def __init__(
        self, replies: list | None = None, lose_final_reply: bool = False
    ) -> None:
        self.replies = replies or []
        self.lose_final_reply = lose_final_reply
        self.off = 0
        self.stored = False
        self.writes: list[int] = []

    def get_max_cbor_and_data_size(self, request) -> tuple[int, int]:
        return 0, 1000

    async def request(self, request, timeout_s=None):
        if isinstance(request, ImageStatesRead):
            stored = b"hash" if self.stored else b"old"
            return SimpleNamespace(images=[SimpleNamespace(hash=stored)])
        self.writes.append(request.off)
        reply = self.replies.pop(0) if self.replies else None
        if isinstance(reply, BaseException):
            raise reply
        if reply is not None:
            self.off = reply
        elif request.off == self.off:
            self.off += len(request.data)
            if self.off == len(IMAGE):
                self.stored = True
                self.off = 0
                if self.lose_final_reply:
                    self.lose_final_reply = False
                    raise TimeoutError
                return SimpleNamespace(off=len(IMAGE), match=True)
        return SimpleNamespace(off=self.off, match=None)


def _udp_upload(device: FakeDevice) -> None:
    asyncio.run(ota_udp._udp_upload(device, IMAGE, PROGRESS, "fd00::1", b"hash"))  # pylint: disable=protected-access


@pytest.mark.parametrize(
    ("device", "writes"),
    [
        # Lost replies are resent
        (
            FakeDevice([TimeoutError(), None, TimeoutError(), None]),
            [0, 0, 1000, 1000, 2000, 3000, 4000, 5000],
        ),
        # A device that restarted the upload is sent the image again from its offset
        (FakeDevice([None, None, 0]), [0, 1000, 2000, 0, 1000, 2000, 3000, 4000, 5000]),
        # The resent final chunk gets offset 0; the image is in the slot, so it is done
        (FakeDevice(lose_final_reply=True), [0, 1000, 2000, 3000, 4000, 5000, 5000]),
    ],
)
def test_udp_upload(smp_ok: None, device: FakeDevice, writes: list[int]) -> None:
    _udp_upload(device)
    assert device.writes == writes


@pytest.mark.parametrize(
    ("replies", "message"),
    [
        ([0] * (ota_udp.SMP_UDP_UPLOAD_RETRIES + 1), "not advancing past offset 0"),
        # Going back to 0 never sets a new high, so repeated restarts stop too
        (
            [None, 0] * (ota_udp.SMP_UDP_UPLOAD_RETRIES + 1),
            "not advancing past offset 1000",
        ),
        (
            [TimeoutError()] * (ota_udp.SMP_UDP_UPLOAD_RETRIES + 1),
            "stalled at offset 0",
        ),
        ([len(IMAGE) + 1], "past"),
    ],
)
def test_udp_upload_gives_up(smp_ok: None, replies: list, message: str) -> None:
    with pytest.raises(EsphomeError, match=message):
        _udp_upload(FakeDevice(replies))


def test_udp_upload_reports_a_hash_mismatch(smp_ok: None) -> None:
    device = FakeDevice()

    async def request(request, timeout_s=None):
        return SimpleNamespace(off=len(IMAGE), match=False)

    device.request = request
    with pytest.raises(EsphomeError, match="mismatched SHA256"):
        _udp_upload(device)


def test_udp_upload_skips_a_late_reply(monkeypatch: pytest.MonkeyPatch) -> None:
    """A reply to a timed-out attempt is still queued when the resend goes out.

    The plain transport would read it as the answer to the resend and stay one
    reply behind for the rest of the upload.
    """
    monkeypatch.setattr(ota_udp, "SMP_UDP_REQUEST_TIMEOUT", 0.05)
    state = {"off": 0, "sent": 0, "late": []}

    def reply(data: bytes) -> list[bytes]:
        state["sent"] += 1
        request = ImageUploadWriteRequest.loads(data)
        if request.off == state["off"]:
            state["off"] += len(request.data)
        frame = bytes(
            ImageUploadWriteResponse(
                sequence=request.header.sequence,
                off=state["off"],
                match=True if state["off"] == len(IMAGE) else None,
            )
        )
        if state["sent"] == 1:
            state["late"].append(frame)  # lost until the resend goes out
            return []
        frames = [*state["late"], frame]
        state["late"].clear()
        return frames

    asyncio.run(
        ota_udp._udp_upload(_queue_client(reply), IMAGE, PROGRESS, "fd00::1", b"hash")  # pylint: disable=protected-access
    )
    # One resend for the lost reply, then one request per chunk
    assert state["sent"] == len(range(0, len(IMAGE), 1252)) + 1


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


def test_udp_no_reply_to_the_first_request_is_unreachable() -> None:
    """The caller tries the next address when the device never answers."""
    client = SimpleNamespace(request=AsyncMock(side_effect=TimeoutError()))
    with pytest.raises(ota_udp.SMPUnreachableError, match="could not be reached"):
        asyncio.run(
            ota._smpmgr_upload_connected(  # pylint: disable=protected-access
                client, "fd00::5", Path("firmware.bin"), b"", True
            )
        )


def test_image_in_slot_logs_why_it_sends_again(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    client = SimpleNamespace(request=AsyncMock(return_value="rc=8"))
    monkeypatch.setattr(ota_udp, "success", lambda response: False)
    assert not asyncio.run(ota_udp._image_in_slot(client, b"hash", "fd00::5"))  # pylint: disable=protected-access
    assert "Could not read the image states" in caplog.text
    assert "rc=8" in caplog.text


@pytest.mark.parametrize("udp", [True, False])
def test_upload_hides_smpclient_timeout_errors_on_udp(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture, udp: bool
) -> None:
    """A recovered timeout logs no smpclient ERROR on UDP; BLE and serial keep it."""
    sent: list[bytes] = []

    def reply(data: bytes) -> list[bytes]:
        sent.append(data)
        if len(sent) == 1:
            return []
        return [bytes(ImageStatesReadResponse(sequence=data[6], images=[]))]

    async def upload(device: str, firmware: Path, udp: bool) -> None:
        client = _queue_client(reply, timeout_s=0.02)
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


class ResetDevice:
    """Plays out each ResetWrite: "ok", "lost" (never arrives) or "applied" (reply lost).

    "rebooting" loses the reply and keeps the device quiet for longer than the short
    wait after a lost reply, as a reset that took effect late does. With swaps=False the device reboots into the old image, as MCUboot does with an
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
        self.quiet_until = 0.0

    async def request(self, request, timeout_s=None):
        if isinstance(request, ota.ResetWrite):
            self.resets += 1
            outcome = self.outcomes.pop(0)
            if outcome == "ok":
                self.new_image = self.swaps
                return SimpleNamespace()
            if outcome == "rebooting":
                self.quiet_until = asyncio.get_running_loop().time() + 0.15
                self.new_image = self.swaps
            self.new_image = self.new_image or outcome == "applied"
            raise TimeoutError
        if asyncio.get_running_loop().time() < self.quiet_until:
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
        # The first reset took effect late, the resend got no reply and the device
        # stayed quiet past the short wait: wait for the reboot, not just warn
        (ResetDevice(["lost", "rebooting"]), 2, "ok"),
        (
            ResetDevice(["lost", "rebooting"], swaps=False),
            2,
            "did not swap in the new image",
        ),
        # Replies lost and the image too large: the old image is reset once more
        (
            ResetDevice(["lost", "ok"], silent=1, swaps=False),
            2,
            "did not swap in the new image",
        ),
    ],
)
def test_udp_reset(
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
    smp_ok: None,
    device: ResetDevice,
    resets: int,
    result: str,
) -> None:
    monkeypatch.setattr(ota_udp, "SMP_UDP_PROBE_TIMEOUT", 0)
    caplog.set_level(logging.INFO)
    monkeypatch.setattr(ota_udp, "SMP_UDP_REBOOT_WAIT", 0.3)
    monkeypatch.setattr(ota_udp, "SMP_UDP_LOST_RESET_WAIT", 0.05)
    run = ota_udp._udp_reset(device, "fd00::5", b"hash")  # pylint: disable=protected-access
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
    confirmed = any(
        r.levelname == "INFO" and r.message.endswith("runs the new image")
        for r in caplog.records
    )
    assert confirmed is (result == "ok")


def test_udp_reset_probe_error_reply(monkeypatch) -> None:
    """An error reply to the probe is a failed read, not a missing image."""
    monkeypatch.setattr(ota_udp, "error", lambda response: False)
    monkeypatch.setattr(ota_udp, "success", lambda response: False)
    monkeypatch.setattr(ota_udp, "SMP_UDP_PROBE_TIMEOUT", 0)
    monkeypatch.setattr(ota_udp, "SMP_UDP_LOST_RESET_WAIT", 0.05)
    device = ResetDevice(["lost"])
    with pytest.raises(EsphomeError, match="answered with an error"):
        asyncio.run(ota_udp._udp_reset(device, "fd00::5", b"hash"))  # pylint: disable=protected-access
    assert device.resets == 1


def _state_client(*images: SimpleNamespace) -> SimpleNamespace:
    return SimpleNamespace(
        request=AsyncMock(return_value=SimpleNamespace(images=list(images)))
    )


def _slot(hash_: bytes, active: bool, pending: bool) -> SimpleNamespace:
    return SimpleNamespace(
        hash=hash_, active=active, pending=pending, confirmed=active, slot=0
    )


@pytest.mark.parametrize("udp", [True, False])
def test_upload_refuses_a_stale_pending_image(smp_ok: None, udp: bool) -> None:
    """An earlier update MCUboot never swapped in blocks the slot on every transport."""
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


def test_upload_keeps_the_same_pending_image(
    monkeypatch: pytest.MonkeyPatch, smp_ok: None
) -> None:
    """Our own pending image is marked and reset again, not refused as stale."""
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
