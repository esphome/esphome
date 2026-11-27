"""Integration test for missing_state on switch, climate and water heater.

These three entity types always sent a concrete state, so a client could not
tell a real OFF from an entity whose value has not been read yet. They now
report missing_state until something publishes, like every other stateful
entity does.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    ClimateInfo,
    ClimateMode,
    ClimateState,
    EntityInfo,
    SwitchInfo,
    SwitchState,
    WaterHeaterInfo,
    WaterHeaterMode,
    WaterHeaterState,
)
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_entity_missing_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a switch, climate and water heater start out unknown.

    This verifies that:
    1. They report missing_state=True while nothing has published
    2. Publishing clears missing_state and reports the published value
    3. A first value that happens to equal the default still publishes
    """
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        switch_info = require_entity(entities, "test_switch", SwitchInfo)
        climate_info = require_entity(entities, "test_climate", ClimateInfo)
        initial_climate_info = require_entity(entities, "initial_climate", ClimateInfo)
        water_heater_info = require_entity(
            entities, "test_water_heater", WaterHeaterInfo
        )
        lambda_water_heater_info = require_entity(
            entities, "lambda_water_heater", WaterHeaterInfo
        )
        publish_button = require_entity(
            entities, "publish_states", description="Publish States button"
        )
        stateful: list[EntityInfo] = [
            switch_info,
            climate_info,
            water_heater_info,
            lambda_water_heater_info,
        ]

        sub = await subscribe_after_initial_states(client, entities)

        # Nothing has published yet, so all of them must report unknown
        for info in stateful:
            state = sub.initial_states.get(info.key)
            assert state is not None, f"No initial state received for {info.object_id}"
            assert state.missing_state is True, (
                f"Initial state for {info.object_id} should have "
                f"missing_state=True, got {state}"
            )

        # A configured initial_state must not start out unknown
        initial_climate_state = sub.initial_states.get(initial_climate_info.key)
        assert isinstance(initial_climate_state, ClimateState)
        assert initial_climate_state.missing_state is False
        assert initial_climate_state.mode is ClimateMode.HEAT

        # Publishing a state on each one clears missing_state
        waits = {info.key: sub.waiter.next_state(info.key) for info in stateful}
        client.button_command(publish_button.key)
        published = dict(zip(waits, await asyncio.gather(*waits.values()), strict=True))

        switch_state = published[switch_info.key]
        assert isinstance(switch_state, SwitchState)
        assert switch_state.missing_state is False
        assert switch_state.state is True

        climate_state = published[climate_info.key]
        assert isinstance(climate_state, ClimateState)
        assert climate_state.missing_state is False
        assert climate_state.mode is ClimateMode.HEAT

        water_heater_state = published[water_heater_info.key]
        assert isinstance(water_heater_state, WaterHeaterState)
        assert water_heater_state.missing_state is False
        assert water_heater_state.mode is WaterHeaterMode.ECO

        # Its first value is OFF, which is also the default the entity starts on,
        # so it only leaves unknown if the first value publishes regardless
        lambda_state = published[lambda_water_heater_info.key]
        assert isinstance(lambda_state, WaterHeaterState)
        assert lambda_state.missing_state is False
        assert lambda_state.mode is WaterHeaterMode.OFF
