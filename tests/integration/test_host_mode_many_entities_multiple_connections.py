"""Integration test for shared buffer optimization with multiple API connections."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_host_mode_many_entities_multiple_connections(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test shared buffer optimization with multiple API connections."""
    # Write, compile and run the ESPHome device
    loop = asyncio.get_running_loop()
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client1,
        api_client_connected() as client2,
    ):
        # Subscribe both clients to state changes
        states1: dict[int, EntityState] = {}
        states2: dict[int, EntityState] = {}

        client1_ready = loop.create_future()
        client2_ready = loop.create_future()

        def on_state1(state: EntityState) -> None:
            states1[state.key] = state
            if len(states1) >= 20 and not client1_ready.done():
                client1_ready.set_result(len(states1))

        def on_state2(state: EntityState) -> None:
            states2[state.key] = state
            if len(states2) >= 20 and not client2_ready.done():
                client2_ready.set_result(len(states2))

        client1.subscribe_states(on_state1)
        client2.subscribe_states(on_state2)

        # Wait for both clients to receive states
        count1, count2 = await wait_or_fail(
            asyncio.gather(client1_ready, client2_ready),
            10.0,
            "One or both clients did not receive enough states",
            client1=lambda: len(states1),
            client2=lambda: len(states2),
        )

        # Verify both clients received states successfully
        assert count1 >= 20, (
            f"Client 1 should have received at least 20 states, got {count1}"
        )
        assert count2 >= 20, (
            f"Client 2 should have received at least 20 states, got {count2}"
        )

        # Verify both clients received the same entity keys (same device state)
        common_keys = set(states1.keys()) & set(states2.keys())
        assert len(common_keys) >= 20, (
            f"Expected at least 20 common entity keys, got {len(common_keys)}"
        )
