"""Integration test for the switch binary sensor, number sensor and text text sensor platforms."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    BinarySensorState,
    EntityState,
    NumberInfo,
    SensorState,
    SwitchInfo,
    TextInfo,
    TextSensorState,
)
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

INITIAL: dict[str, tuple[type[EntityState], float | bool | str | None]] = {
    "switch_state": (BinarySensorState, True),
    # A switch that has not published leaves its binary sensor unknown
    "quiet_switch_state": (BinarySensorState, None),
    "number_value": (SensorState, 4.0),
    "text_value": (TextSensorState, "hello"),
}
UPDATED: dict[str, tuple[type[EntityState], float | bool | str | None]] = {
    "switch_state": (BinarySensorState, False),
    "quiet_switch_state": (BinarySensorState, True),
    "number_value": (SensorState, 6.5),
    "text_value": (TextSensorState, "bye"),
}


@pytest.mark.asyncio
async def test_source_entity_follow(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each entity holds its source's state from boot, then follows it."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        keys = {
            name: require_entity(entities, name, exact=True).key for name in INITIAL
        }
        switch = require_entity(entities, "source_switch", SwitchInfo)
        quiet = require_entity(entities, "quiet_switch", SwitchInfo)
        number = require_entity(entities, "source_number", NumberInfo)
        text = require_entity(entities, "source_text", TextInfo)

        sub = await subscribe_after_initial_states(client, entities)
        for name, (cls, state) in INITIAL.items():
            key = keys[name]
            expected = (
                cls(key=key, missing_state=True)
                if state is None
                else cls(key=key, state=state)
            )
            assert sub.initial_states[key] == expected, name

        names = list(UPDATED)
        waits = [sub.waiter.next_state(keys[name]) for name in names]
        client.switch_command(switch.key, False)
        client.switch_command(quiet.key, True)
        client.number_command(number.key, 6.5)
        client.text_command(text.key, "bye")
        for name, got in zip(names, await asyncio.gather(*waits), strict=True):
            cls, state = UPDATED[name]
            assert got == cls(key=keys[name], state=state), name
