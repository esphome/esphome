"""Integration test for the camera API flow using a mock camera platform."""

from __future__ import annotations

import asyncio

from aioesphomeapi import CameraInfo, CameraState, EntityState
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

# Must match image_size in fixtures/camera_mock.yaml
IMAGE_SIZE = 4096
STREAM_FRAMES = 3


def _verify_frame(data: bytes) -> int:
    """Verify the deterministic frame pattern and return the frame counter."""
    assert len(data) == IMAGE_SIZE, f"expected {IMAGE_SIZE} bytes, got {len(data)}"
    counter = data[0]
    assert data == bytes((counter + i) & 0xFF for i in range(IMAGE_SIZE)), (
        "frame pattern mismatch"
    )
    return counter


@pytest.mark.asyncio
async def test_camera_mock(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Single-image and stream requests deliver reassembled deterministic frames."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        camera = require_entity(entities, "mock_camera", CameraInfo)

        loop = asyncio.get_running_loop()
        images: list[bytes] = []
        single_image: asyncio.Future[None] = loop.create_future()
        stream_done: asyncio.Future[None] = loop.create_future()

        def on_state(state: EntityState) -> None:
            if not (isinstance(state, CameraState) and state.key == camera.key):
                return
            images.append(bytes(state.data))
            if not single_image.done():
                single_image.set_result(None)
            elif len(images) >= STREAM_FRAMES and not stream_done.done():
                stream_done.set_result(None)

        client.subscribe_states(on_state)

        # Single image request: one complete frame arrives, reassembled
        # from multiple chunks (4096 > 1390 byte packets)
        client.request_single_image()
        await asyncio.wait_for(single_image, timeout=10)
        first_counter = _verify_frame(images[0])

        # Stream request: multiple consecutive frames arrive
        images.clear()
        client.request_image_stream()
        await asyncio.wait_for(stream_done, timeout=10)

        # Frames are distinct, ordered, and fresh per the mock's counter.
        # Not exactly consecutive: the API drops frames by design while the
        # previous image is still being sent, so allow small gaps.
        counters = [_verify_frame(img) for img in images[:STREAM_FRAMES]]
        for prev, cur in zip(counters, counters[1:], strict=False):
            assert cur != prev, f"duplicate frames: {counters}"
            assert ((cur - prev) & 0xFF) < 16, f"frames out of order: {counters}"
        assert counters[0] != first_counter, "stream should produce new frames"
