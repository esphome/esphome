"""Integration test for the api on_client_synced trigger."""

from __future__ import annotations

import asyncio

from aioesphomeapi import HomeassistantServiceCall
import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_client_synced(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The trigger fires after the initial states, with the action subscription active."""
    waiter = LineWaiter()
    calls: asyncio.Queue[HomeassistantServiceCall] = asyncio.Queue()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        assert waiter.count("Synced trigger entered") == 0

        client.subscribe_home_assistant_states_and_services(
            on_state=lambda _: None,
            on_service_call=calls.put_nowait,
            on_state_sub=lambda *_: None,
        )
        await waiter.wait_for_each(
            "Synced trigger entered", "Synced trigger fired", timeout=5.0
        )
        call = await asyncio.wait_for(calls.get(), timeout=5.0)
        assert call.service == "esphome.client_synced"
        assert call.is_event
