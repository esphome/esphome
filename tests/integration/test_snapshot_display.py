"""Integration test for the snapshot display and the file writing shared with other displays."""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import LogLevel
import pytest

from .bmp_utils import capture_when_drawn, wait_for_bmp
from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 101
HEIGHT = 64

# Part of the message the writer logs when it will not write over a file that is already there.
REFUSAL_MESSAGE = b"not overwriting"


@pytest.mark.asyncio
async def test_snapshot_display(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A display with no screen draws into memory and writes what it drew to a file."""
    snapshot_dir = tmp_path / "snapshots"
    # The device reads this when it writes a file; the subprocess inherits our environment, so it
    # must be set before the binary is launched.
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        service = next(s for s in services if s.name == "take_snapshot")

        async def take(name: str) -> None:
            await client.execute_service(service, {"name": name})

        # The test card is drawn in several colours, so once it is on the screen the picture is
        # not one flat shade. Capturing until that is true waits out the first update rather than
        # racing it.
        image, capture = await capture_when_drawn(take, snapshot_dir)
        assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)

        # An extension is only added when there is not one already, whatever its case.
        await take("UPPER.BMP")
        await wait_for_bmp(snapshot_dir / "UPPER.BMP")

        # A name that tries to lead somewhere else is cut back to one harmless name in the
        # snapshot directory.
        await take("../escape")
        await wait_for_bmp(snapshot_dir / ".._escape.bmp")

        # A second capture under a name already used must fail rather than write over the first.
        # Wait for the device to report the refusal: on its own, an unchanged file cannot tell a
        # refusal apart from a request the device has not got to yet, so a regression that wrote
        # over the file could still pass on a busy machine.
        refused = asyncio.Event()

        def on_log(msg) -> None:
            if REFUSAL_MESSAGE in msg.message:
                refused.set()

        client.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)

        before = capture.read_bytes()
        await take(capture.name)
        await asyncio.wait_for(refused.wait(), timeout=10.0)
        assert capture.read_bytes() == before
        # Nothing beyond what was asked for, leaving out however many captures it took to wait
        # for the first frame.
        written = sorted(
            p.name for p in snapshot_dir.iterdir() if not p.name.startswith("drawn-")
        )
        assert written == [".._escape.bmp", "UPPER.BMP"]
