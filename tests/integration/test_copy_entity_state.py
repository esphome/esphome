"""Integration test for the copy sensor, binary sensor and text sensor platforms."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    ButtonInfo,
    EntityState,
    SensorInfo,
    SensorState,
    TextSensorInfo,
    TextSensorState,
)
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_copy_entity_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Copies hold the state their source published before and after their setup, then follow it."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        early_sensor = require_entity(entities, "copy_early_sensor", SensorInfo)
        late_sensor = require_entity(entities, "copy_late_sensor", SensorInfo)
        early_binary = require_entity(entities, "copy_early_binary", BinarySensorInfo)
        late_binary = require_entity(entities, "copy_late_binary", BinarySensorInfo)
        early_text = require_entity(entities, "copy_early_text", TextSensorInfo)
        late_text = require_entity(entities, "copy_late_text", TextSensorInfo)
        button = require_entity(entities, "update_sources", ButtonInfo)

        sub = await subscribe_after_initial_states(client, entities)
        initial = sub.initial_states
        assert initial[early_sensor.key] == SensorState(key=early_sensor.key, state=1.5)
        assert initial[late_sensor.key] == SensorState(key=late_sensor.key, state=2.5)
        assert initial[early_binary.key] == BinarySensorState(
            key=early_binary.key, state=True
        )
        assert initial[late_binary.key] == BinarySensorState(
            key=late_binary.key, state=True
        )
        assert initial[early_text.key] == TextSensorState(
            key=early_text.key, state="early"
        )
        assert initial[late_text.key] == TextSensorState(
            key=late_text.key, state="late"
        )

        waits: dict[int, asyncio.Future[EntityState]] = {
            key: sub.waiter.next_state(key)
            for key in (late_sensor.key, early_binary.key, late_text.key)
        }
        client.button_command(button.key)
        states = dict(zip(waits, await asyncio.gather(*waits.values()), strict=True))
        assert states[late_sensor.key] == SensorState(key=late_sensor.key, state=7.25)
        assert states[early_binary.key] == BinarySensorState(
            key=early_binary.key, state=False
        )
        assert states[late_text.key] == TextSensorState(
            key=late_text.key, state="updated"
        )
