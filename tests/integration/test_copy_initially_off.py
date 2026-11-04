"""Integration test for a copy binary sensor whose source starts off."""

from __future__ import annotations

from aioesphomeapi import BinarySensorInfo, BinarySensorState, SensorInfo, SensorState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_copy_initially_off(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The copy takes the source's constructor state and its on_state_change sees it."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        copy = require_entity(entities, "copy_binary", BinarySensorInfo)
        count = require_entity(entities, "change_count", SensorInfo)

        sub = await subscribe_after_initial_states(client, entities)
        assert sub.initial_states[copy.key] == BinarySensorState(
            key=copy.key, state=False
        )
        assert sub.initial_states[count.key] == SensorState(key=count.key, state=1.0)
