"""Integration test for noise session resume."""

from __future__ import annotations

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

    device_lines: list[str] = []

    async with (
        run_compiled(yaml_config, line_callback=device_lines.append),
        api_client_connected(noise_psk=NOISE_KEY) as client,
    ):
        # First connection: full handshake, the device issues a ticket
        info = await client.device_info()
        assert info.name == "host-noise-resume"
        assert not any("Session resumed" in line for line in device_lines)

        # Same client reconnects and offers the ticket
        await client.disconnect()
        await client.connect(login=True)
        info = await client.device_info()
        assert info.name == "host-noise-resume"
        assert any("Session resumed" in line for line in device_lines)

        # The resumed session issued a fresh ticket, so it resumes again
        await client.disconnect()
        await client.connect(login=True)
        info = await client.device_info()
        assert info.name == "host-noise-resume"
        assert sum("Session resumed" in line for line in device_lines) == 2
