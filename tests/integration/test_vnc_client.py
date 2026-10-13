"""Integration test for the vnc component against a fake RFB server.

One compiled device runs every step in order against one server: the first frame, request
pacing, a reconnect after the server closes the link, a stream cut into tiny pieces, and an
unknown message type that makes the client drop the link.

The client asks for the next frame only once the previous one is fully drawn, so snapshots are
taken after that request arrives. The snapshot display draws as soon as pixels arrive, so the
picture does not depend on when the client refreshes the display.
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import contextlib
from dataclasses import asdict
from itertools import pairwise
from pathlib import Path
import random
import re
import struct

from aioesphomeapi import APIClient, UserService
import pytest

from .bmp_utils import Bmp, wait_for_bmp
from .rfb_fake_server import (
    HOSTILE_CHUNK,
    RGB,
    RGB565_LE,
    FakeRfbServer,
    Message,
    PixelFormat,
    Rect,
)
from .types import APIClientConnectedFactory, RunCompiledFunction

DISPLAY_WIDTH = 64
DISPLAY_HEIGHT = 48
SERVER_WIDTH = 80
SERVER_HEIGHT = 60

FIXTURE_PORT = "port: 15900"
CONNECTED = "VNC_TEST_CONNECTED"
DISCONNECTED = "VNC_TEST_DISCONNECTED"
FRAME_LOG = re.compile(r"Update: (\d+) bytes in \d+ ?ms")
LONG_OPERATION = "vnc took a long time"

# Well under the fixture's 300 ms update_delay: the next request must not wait for the refresh.
QUICK_REQUEST_S = 0.25
# Requests are at least 50 ms apart; leave room for jitter in when they arrive.
REQUEST_SPACING_S = 0.035
# The fixture's reconnect_interval.
RECONNECT_S = 1.0
# Long enough for a frame that drew something to be logged.
LOG_SETTLE_S = 0.5

# Raw rects that cross the right and bottom edges of the display.
FULL_FRAME: list[Rect] = [
    (0, 0, SERVER_WIDTH, 13, "raw"),
    (0, 13, 37, SERVER_HEIGHT - 13, "raw"),
    (37, 13, SERVER_WIDTH - 37, SERVER_HEIGHT - 13, "raw"),
]

LogMatch = str | re.Pattern[str] | Callable[[str], bool]


class LogWatch:
    """Device log lines with the time each one arrived."""

    def __init__(self) -> None:
        self.lines: list[tuple[float, str]] = []
        self._changed = asyncio.Event()

    def callback(self, line: str) -> None:
        self.lines.append((asyncio.get_running_loop().time(), line))
        self._changed.set()
        self._changed = asyncio.Event()

    def times(self, match: LogMatch) -> list[float]:
        """When each matching line came."""
        if isinstance(match, str):
            text = match

            def test(line: str) -> bool:
                return text in line

        elif isinstance(match, re.Pattern):
            pattern = match

            def test(line: str) -> bool:
                return pattern.search(line) is not None

        else:
            test = match
        return [when for when, line in self.lines if test(line)]

    def count(self, match: LogMatch) -> int:
        return len(self.times(match))

    def frame_sizes(self) -> list[int]:
        """The byte counts of the 'Update:' lines, in order."""
        return [
            int(found[1]) for _, line in self.lines if (found := FRAME_LOG.search(line))
        ]

    async def wait_count(
        self, match: LogMatch, count: int, timeout: float = 10.0
    ) -> float:
        """Wait until ``match`` was logged ``count`` times and return when the last one came."""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while len(times := self.times(match)) < count:
            if (remaining := deadline - loop.time()) <= 0:
                raise AssertionError(
                    f"{match!r} logged {len(times)} times within {timeout}s, "
                    f"expected {count}"
                )
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(self._changed.wait(), remaining)
        return times[count - 1]


def paint(server: FakeRfbServer, seed: int) -> None:
    """Draw solid bands, boxes and noise.

    Noise also runs along the right and bottom edges of the display, so clipping errors show.
    """
    rng = random.Random(seed)

    def colour() -> RGB:
        return rng.randrange(256), rng.randrange(256), rng.randrange(256)

    width, height = server.width, server.height
    for x in range(0, width, 20):
        server.fill_rect(x, 0, min(20, width - x), height, colour())
    accent = colour()
    for _ in range(10):
        server.fill_rect(
            rng.randrange(width - 8),
            rng.randrange(height - 8),
            rng.randint(1, 8),
            rng.randint(1, 8),
            accent,
        )
    for _ in range(10):
        server.fill_rect(
            rng.randrange(width - 4),
            rng.randrange(height - 4),
            rng.randint(1, 4),
            rng.randint(1, 4),
            colour(),
        )
    noise = [
        (x, y)
        for x in range(DISPLAY_WIDTH - 6, min(width, DISPLAY_WIDTH + 6))
        for y in range(height)
    ]
    noise += [
        (x, y)
        for y in range(DISPLAY_HEIGHT - 5, min(height, DISPLAY_HEIGHT + 5))
        for x in range(width)
    ]
    noise += [(x, y) for x in range(8, 24) for y in range(16, 32)]
    for x, y in noise:
        server.set_pixel(x, y, colour())


def shown_colour(fmt: PixelFormat, rgb: RGB) -> RGB:
    """The colour a snapshot holds for a server pixel, after the trip through RGB565."""
    value = fmt.value_of(rgb)
    return (
        ((value >> fmt.red_shift) & fmt.red_max) * 255 // fmt.red_max,
        ((value >> fmt.green_shift) & fmt.green_max) * 255 // fmt.green_max,
        ((value >> fmt.blue_shift) & fmt.blue_max) * 255 // fmt.blue_max,
    )


def assert_shows(bmp: Bmp, server: FakeRfbServer) -> None:
    """Assert the snapshot is the top-left of the server screen, pixel for pixel."""
    assert (bmp.width, bmp.height, bmp.bits) == (DISPLAY_WIDTH, DISPLAY_HEIGHT, 24)
    mismatches = []
    for y in range(DISPLAY_HEIGHT):
        row = (DISPLAY_HEIGHT - 1 - y) * DISPLAY_WIDTH * 3
        for x in range(DISPLAY_WIDTH):
            b, g, r = bmp.pixels[row + x * 3 : row + x * 3 + 3]
            expected = shown_colour(RGB565_LE, server.get_pixel(x, y))
            if (r, g, b) != expected:
                mismatches.append(((x, y), (r, g, b), expected))
    assert not mismatches, (
        f"{len(mismatches)} pixels differ; (x, y), shown, expected: {mismatches[:10]}"
    )


def full_request(incremental: bool) -> Message:
    return {
        "type": "fbur",
        "incremental": incremental,
        "x": 0,
        "y": 0,
        "w": DISPLAY_WIDTH,
        "h": DISPLAY_HEIGHT,
    }


def cut_text(text: bytes) -> bytes:
    return struct.pack(">B3xI", 3, len(text)) + text


class Device:
    """The API actions of the fixture."""

    def __init__(
        self, client: APIClient, services: list[UserService], snapshot_dir: Path
    ) -> None:
        self.client = client
        self.actions = {service.name: service for service in services}
        self.snapshot_dir = snapshot_dir

    async def run(self, action: str, **data: int | str) -> None:
        await self.client.execute_service(self.actions[action], data)

    async def snapshot(self, name: str) -> Bmp:
        await self.run("take_snapshot", filename=name)
        return await wait_for_bmp(self.snapshot_dir / f"{name}.bmp")


async def next_request(
    server: FakeRfbServer, timeout: float = 5.0
) -> tuple[Message, float]:
    """Wait for the next FramebufferUpdateRequest and return it and when it came."""
    request = await server.wait_for("fbur", timeout=timeout)
    return request, asyncio.get_running_loop().time()


async def exchange(
    server: FakeRfbServer,
    data: bytes,
    *,
    log: LogWatch | None = None,
    frame: bytes | None = None,
    chunk: int | None = None,
    timeout: float = 5.0,
) -> tuple[float, float]:
    """Send bytes that end a frame and wait for the incremental request that follows.

    With ``log``, also wait for the frame's 'Update:' line and check its byte count; ``frame``
    is the FramebufferUpdate inside ``data`` when other messages surround it.

    Returns how long after the write the request came, and when it came.
    """
    logged = log.count(FRAME_LOG) if log is not None else 0
    await server.send_raw(data, chunk)
    sent = asyncio.get_running_loop().time()
    request, requested = await next_request(server, timeout)
    assert request == full_request(incremental=True)
    if log is not None:
        await expect_logged(log, data if frame is None else frame, logged)
    return requested - sent, requested


async def expect_logged(log: LogWatch, frame: bytes, before: int) -> None:
    """Wait for the 'Update:' line after the first ``before`` and check its byte count."""
    await log.wait_count(FRAME_LOG, before + 1)
    sizes = log.frame_sizes()
    assert len(sizes) == before + 1, f"expected {before + 1} logged frames: {sizes}"
    # The four byte message header may or may not be counted.
    assert sizes[-1] in (len(frame) - 4, len(frame)), (
        f"logged {sizes[-1]} bytes for a {len(frame)} byte update"
    )


@pytest.mark.asyncio
async def test_vnc_client(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The client renders, paces and recovers from drops and bad streams."""
    snapshot_dir = tmp_path / "snapshots"
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))
    loop = asyncio.get_running_loop()
    log = LogWatch()
    server = FakeRfbServer(SERVER_WIDTH, SERVER_HEIGHT, name="vnc client test")
    paint(server, seed=1)

    async with server:
        assert FIXTURE_PORT in yaml_config
        yaml_config = yaml_config.replace(FIXTURE_PORT, f"port: {server.port}")
        async with (
            run_compiled(yaml_config, line_callback=log.callback),
            api_client_connected() as client,
        ):
            _, services = await client.list_entities_services()
            device = Device(client, services, snapshot_dir)

            # Render: the session setup is exact and the first frame is pixel-exact.
            pixel_format = await server.wait_for("set_pixel_format", timeout=15.0)
            assert pixel_format == {"type": "set_pixel_format", **asdict(RGB565_LE)}
            encodings = await server.wait_for("set_encodings")
            assert encodings["encodings"] == [0, -239]
            request, _ = await next_request(server)
            assert request == full_request(incremental=False)
            assert [m["type"] for m in server.messages[:3]] == [
                "set_pixel_format",
                "set_encodings",
                "fbur",
            ]
            assert server.client_version == b"RFB 003.008\n"
            assert server.shared_flag == 1
            await log.wait_count(CONNECTED, 1)
            assert log.count("Server 'vnc client test', 80x60, RFB 3.8") == 1
            assert log.count("showing the top-left 64x48") == 1

            elapsed, _ = await exchange(
                server, server.build_update(FULL_FRAME), log=log
            )
            assert elapsed < QUICK_REQUEST_S, f"next request after {elapsed:.3f}s"
            assert_shows(await device.snapshot("render"), server)

            # Pacing: one request stays parked while nothing changes.
            await asyncio.sleep(1.0)
            assert not [m for m in server.pending() if m["type"] == "fbur"]

            # The next request goes out as soon as a frame is drawn, before the refresh.
            server.fill_rect(30, 10, 10, 10, (250, 10, 10))
            server.fill_rect(58, 40, 22, 20, (10, 250, 10))
            frame = server.build_update(
                [(30, 10, 10, 10, "raw"), (58, 40, 22, 20, "raw")]
            )
            elapsed, _ = await exchange(server, frame, log=log)
            assert elapsed < QUICK_REQUEST_S, f"next request after {elapsed:.3f}s"
            assert_shows(await device.snapshot("pacing"), server)

            # A frame that only changes what the display cannot show draws nothing.
            logged = log.count(FRAME_LOG)
            server.fill_rect(
                DISPLAY_WIDTH, 0, SERVER_WIDTH - DISPLAY_WIDTH, SERVER_HEIGHT, (1, 2, 3)
            )
            server.fill_rect(
                0,
                DISPLAY_HEIGHT,
                SERVER_WIDTH,
                SERVER_HEIGHT - DISPLAY_HEIGHT,
                (200, 100, 50),
            )
            hidden = server.build_update(
                [
                    (
                        DISPLAY_WIDTH,
                        0,
                        SERVER_WIDTH - DISPLAY_WIDTH,
                        SERVER_HEIGHT,
                        "raw",
                    ),
                    (
                        0,
                        DISPLAY_HEIGHT,
                        SERVER_WIDTH,
                        SERVER_HEIGHT - DISPLAY_HEIGHT,
                        "raw",
                    ),
                ]
            )
            elapsed, requested = await exchange(server, hidden)
            assert elapsed < QUICK_REQUEST_S, f"next request after {elapsed:.3f}s"

            # Empty and cursor-only updates draw nothing either, and requests stay 50 ms
            # apart.
            requested_at = [requested]
            for update in (
                server.build_update([]),
                *(
                    server.build_update([(2 + i, 3, 10, 16, "rich-cursor")])
                    for i in range(3)
                ),
            ):
                elapsed, requested = await exchange(server, update)
                assert elapsed < QUICK_REQUEST_S, f"next request after {elapsed:.3f}s"
                requested_at.append(requested)
            gaps = [later - earlier for earlier, later in pairwise(requested_at)]
            assert min(gaps) >= REQUEST_SPACING_S, f"requests {gaps} apart"
            await asyncio.sleep(LOG_SETTLE_S)
            assert log.count(FRAME_LOG) == logged, (
                "a frame that drew nothing was logged"
            )
            assert_shows(await device.snapshot("hidden"), server)

            # Reconnect: the server closes the link; the client comes back after the backoff
            # and sets the session up again.
            server.discard_pending()
            closed = loop.time()
            await server.close_client()
            await log.wait_count(DISCONNECTED, 1)
            await server.wait_until(
                lambda: server.connections == 2, timeout=10.0, what="second connection"
            )
            reconnected = loop.time()
            assert reconnected - closed >= RECONNECT_S - 0.05, (
                f"reconnected after {reconnected - closed:.3f}s"
            )
            await log.wait_count(CONNECTED, 2)
            assert await server.wait_for("set_pixel_format") == pixel_format
            assert (await server.wait_for("set_encodings"))["encodings"] == [0, -239]
            request, _ = await next_request(server)
            assert request == full_request(incremental=False)
            assert server.sessions == 2

            # Hostile stream: 1-7 byte pieces, a long cut text, bells, a colour map and
            # cursor rects around a frame.
            paint(server, seed=2)
            frame = server.build_update(
                [
                    (3, 4, 12, 18, "rich-cursor"),
                    (0, 0, DISPLAY_WIDTH, 20, "raw"),
                    (DISPLAY_WIDTH, 0, SERVER_WIDTH - DISPLAY_WIDTH, 60, "raw"),
                    (0, 0, 0, 0, "rich-cursor"),
                    (0, 20, 30, SERVER_HEIGHT - 20, "raw"),
                    (30, 20, SERVER_WIDTH - 30, SERVER_HEIGHT - 20, "raw"),
                    (1, 1, 7, 9, "rich-cursor"),
                ]
            )
            colour_map = struct.pack(">BxHH", 1, 0, 2) + bytes(12)
            long_cut = bytes(
                random.Random(3).randrange(32, 127) for _ in range(200_000)
            )
            payload = (
                cut_text(long_cut)
                + b"\x02"
                + colour_map
                + frame
                + b"\x02"
                + cut_text(b"")
            )
            await exchange(
                server, payload, log=log, frame=frame, chunk=HOSTILE_CHUNK, timeout=30.0
            )
            assert_shows(await device.snapshot("hostile"), server)

            # An unknown message type drops the link. This session was too short to count as
            # stable, so the backoff has doubled.
            server.discard_pending()
            sent = loop.time()
            await server.send_raw(b"\x09")
            await log.wait_count(DISCONNECTED, 2)
            assert log.count("Unknown message type 9") == 1
            await server.wait_until(
                lambda: server.connections == 3, timeout=15.0, what="third connection"
            )
            assert loop.time() - sent >= 2 * RECONNECT_S - 0.1, (
                f"reconnected after {loop.time() - sent:.3f}s"
            )
            await log.wait_count(CONNECTED, 3)
            request, _ = await next_request(server)
            assert request == full_request(incremental=False)
            paint(server, seed=6)
            await exchange(server, server.build_update(FULL_FRAME), log=log)
            assert_shows(await device.snapshot("third"), server)

            assert server.protocol_errors == []
            assert log.count(LONG_OPERATION) == 0
