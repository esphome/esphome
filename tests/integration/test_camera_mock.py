"""Integration test for the camera API flow using a mock camera platform."""

from __future__ import annotations

import asyncio

from aioesphomeapi import CameraInfo, CameraState, EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

IMAGE_SIZE = 4096


def _verify_frame(data: bytes) -> int:
    """Verify the deterministic frame pattern and return the frame counter."""
    assert len(data) == IMAGE_SIZE, f"expected {IMAGE_SIZE} bytes, got {len(data)}"
    counter = data[0]
    for i, byte in enumerate(data):
        expected = (counter + i) & 0xFF
        assert byte == expected, f"byte {i}: expected {expected}, got {byte}"
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
        cameras = [e for e in entities if isinstance(e, CameraInfo)]
        assert len(cameras) == 1, "expected exactly one camera entity"
        camera = cameras[0]
        assert camera.name == "Mock Camera"

        loop = asyncio.get_running_loop()
        images: list[bytes] = []
        image_received = asyncio.Event()

        def on_state(state: EntityState) -> None:
            if isinstance(state, CameraState) and state.key == camera.key:
                images.append(bytes(state.data))
                image_received.set()

        client.subscribe_states(on_state)

        # Single image request: one complete frame arrives, reassembled
        # from multiple chunks (4096 > 1390 byte packets)
        client.request_single_image()
        await asyncio.wait_for(image_received.wait(), timeout=10)
        first_counter = _verify_frame(images[0])

        # Stream request: multiple consecutive frames arrive
        images.clear()
        image_received.clear()
        client.request_image_stream()
        deadline = loop.time() + 10
        while len(images) < 3 and loop.time() < deadline:
            image_received.clear()
            try:
                await asyncio.wait_for(image_received.wait(), timeout=10)
            except TimeoutError:
                break
        assert len(images) >= 3, f"expected at least 3 stream frames, got {len(images)}"

        # Frames are distinct and sequential per the mock's counter
        counters = [_verify_frame(img) for img in images[:3]]
        for prev, cur in zip(counters, counters[1:], strict=False):
            assert cur == (prev + 1) & 0xFF, f"non-sequential frames: {counters}"
        assert counters[0] != first_counter, "stream should produce new frames"
