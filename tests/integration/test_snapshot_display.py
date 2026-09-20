"""Integration test for the snapshot display and the file writing shared with other displays."""

from __future__ import annotations

import asyncio
import io
from pathlib import Path

from aioesphomeapi import LogLevel
from PIL import Image, UnidentifiedImageError
import pytest

from .bmp_utils import capture_when_drawn, wait_for_bmp
from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 101
HEIGHT = 64

ANIMATION_FRAMES = 5
# The fixture asks for 20 frames a second, and a GIF counts time in milliseconds here.
ANIMATION_FRAME_MS = 50

# Part of the message the writer logs when it will not write over a file that is already there.
REFUSAL_MESSAGE = b"not overwriting"


async def wait_for_gif(path: Path, frames: int, timeout: float = 5.0) -> Image.Image:
    """Wait for a complete animated GIF with the given number of frames and return it.

    The file exists from the moment the recording starts and grows a frame at a time, so keep
    reading until it holds all of them and ends with the GIF trailer.
    """
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while loop.time() < deadline:
        try:
            data = path.read_bytes()
            # Open the bytes just read, so the trailer check and the decode see the same file.
            image = Image.open(io.BytesIO(data))
            if data.endswith(b";") and image.n_frames == frames:
                # Decoding every frame proves the compressed data is all there and valid.
                for frame in range(frames):
                    image.seek(frame)
                    image.load()
                image.seek(0)
                return image
        except (FileNotFoundError, UnidentifiedImageError, OSError, EOFError):
            pass
        await asyncio.sleep(0.05)
    raise AssertionError(
        f"no complete {frames} frame GIF appeared at {path} within {timeout}s"
    )


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

        animation_service = next(s for s in services if s.name == "take_animation")

        # The test card is drawn in several colours, so once it is on the screen the picture is
        # not one flat shade. Capturing until that is true waits out the first update rather than
        # racing it.
        image, capture = await capture_when_drawn(take, snapshot_dir)
        assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)

        # Asking for frames records a GIF: every frame is the size of the display and lasts as long
        # as the frame rate says. The test card does not move, so every frame is the picture
        # captured above.
        await client.execute_service(animation_service, {"name": "movie"})
        movie = await wait_for_gif(snapshot_dir / "movie.gif", ANIMATION_FRAMES)
        assert movie.size == (WIDTH, HEIGHT)
        expected = Image.frombytes(
            "RGB", (WIDTH, HEIGHT), image.pixels, "raw", "BGR", 0, -1
        )
        for frame in range(ANIMATION_FRAMES):
            movie.seek(frame)
            assert movie.info["duration"] == ANIMATION_FRAME_MS
            assert movie.convert("RGB").tobytes() == expected.tobytes()

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
        assert written == [".._escape.bmp", "UPPER.BMP", "movie.gif"]
