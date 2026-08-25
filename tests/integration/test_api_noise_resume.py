"""Integration test for noise session resume."""

from __future__ import annotations

import asyncio

import aioesphomeapi.core
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

NOISE_KEY = "N4Yle5YirwZhPiHHsdZLdOA73ndj/84veVaLhTvxCuU="


@pytest.mark.asyncio
async def test_api_noise_resume(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A reconnect with the ticket from the first connection resumes the session."""
    if not hasattr(aioesphomeapi.core, "ResumeAPIError"):
        pytest.skip("aioesphomeapi without noise session resume")

    resumed = asyncio.Event()
    resumed_count = 0

    def on_line(line: str) -> None:
        nonlocal resumed_count
        if "Session resumed" in line:
            resumed_count += 1
            resumed.set()

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected(noise_psk=NOISE_KEY) as client,
    ):
        # First connection: full handshake, the device issues a ticket
        info = await client.device_info()
        assert info.name == "host-noise-resume"
        assert resumed_count == 0

        # Same client reconnects and offers the ticket
        await client.disconnect()
        await client.connect(login=True)
        info = await client.device_info()
        assert info.name == "host-noise-resume"
        await asyncio.wait_for(resumed.wait(), timeout=10.0)
        assert resumed_count == 1
        resumed.clear()

        # The resumed session issued a fresh ticket, so it resumes again
        await client.disconnect()
        await client.connect(login=True)
        info = await client.device_info()
        assert info.name == "host-noise-resume"
        await asyncio.wait_for(resumed.wait(), timeout=10.0)
        assert resumed_count == 2
