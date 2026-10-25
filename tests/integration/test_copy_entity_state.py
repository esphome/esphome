"""Integration test for the copy sensor, binary sensor and text sensor platforms."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    BinarySensorState,
    ButtonInfo,
    EntityState,
    SensorState,
    TextSensorState,
)
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

INITIAL: dict[str, tuple[type[EntityState], float | bool | str]] = {
    "copy_early_sensor": (SensorState, 1.5),
    "copy_late_sensor": (SensorState, 2.5),
    "copy_early_binary": (BinarySensorState, True),
    # The first state reaches no on_state callback by default
    "copy_late_binary": (BinarySensorState, True),
    "copy_early_text": (TextSensorState, "early"),
    "copy_late_text": (TextSensorState, "late"),
}
UPDATED: dict[str, tuple[type[EntityState], float | bool | str]] = {
    "copy_late_sensor": (SensorState, 7.25),
    "copy_early_binary": (BinarySensorState, False),
    "copy_late_text": (TextSensorState, "updated"),
}


@pytest.mark.asyncio
async def test_copy_entity_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Copies hold the state their source published before and after their setup, then follow it."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        keys = {name: require_entity(entities, name).key for name in INITIAL}
        button = require_entity(entities, "update_sources", ButtonInfo)

        sub = await subscribe_after_initial_states(client, entities)
        for name, (cls, state) in INITIAL.items():
            key = keys[name]
            assert sub.initial_states[key] == cls(key=key, state=state), name

        names = list(UPDATED)
        waits = [sub.waiter.next_state(keys[name]) for name in names]
        client.button_command(button.key)
        for name, got in zip(names, await asyncio.gather(*waits), strict=True):
            cls, state = UPDATED[name]
            assert got == cls(key=keys[name], state=state), name
