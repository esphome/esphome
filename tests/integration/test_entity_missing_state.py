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
    EntityState,
    SwitchInfo,
    SwitchState,
    WaterHeaterInfo,
    WaterHeaterMode,
    WaterHeaterState,
)
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

# The device sends the field either way; this only reads it back. Drop the skip once
# requirements.txt pins an aioesphomeapi that carries it on these three state models.
pytestmark = pytest.mark.skipif(
    "missing_state" not in SwitchState.__dataclass_fields__,
    reason="aioesphomeapi does not carry missing_state on these states yet",
)


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
    loop = asyncio.get_running_loop()
    futures: dict[int, asyncio.Future[EntityState]] = {}

    def on_state(state: EntityState) -> None:
        """Resolve the pending future for the entity that changed."""
        future = futures.get(state.key)
        if future is not None and not future.done():
            future.set_result(state)

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

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Nothing has published yet, so all of them must report unknown
        for info in stateful:
            state = initial_state_helper.initial_states.get(info.key)
            assert state is not None, f"No initial state received for {info.object_id}"
            assert state.missing_state is True, (
                f"Initial state for {info.object_id} should have "
                f"missing_state=True, got {state}"
            )

        # A configured initial_state must not start out unknown
        initial_climate_state = initial_state_helper.initial_states.get(
            initial_climate_info.key
        )
        assert isinstance(initial_climate_state, ClimateState)
        assert initial_climate_state.missing_state is False
        assert initial_climate_state.mode is ClimateMode.HEAT

        # Publishing a state on each one clears missing_state
        futures = {info.key: loop.create_future() for info in stateful}
        client.button_command(publish_button.key)

        try:
            await asyncio.wait_for(asyncio.gather(*futures.values()), timeout=5.0)
        except TimeoutError:
            pytest.fail("Timeout waiting for published states")

        switch_state = futures[switch_info.key].result()
        assert isinstance(switch_state, SwitchState)
        assert switch_state.missing_state is False
        assert switch_state.state is True

        climate_state = futures[climate_info.key].result()
        assert isinstance(climate_state, ClimateState)
        assert climate_state.missing_state is False
        assert climate_state.mode is ClimateMode.HEAT

        water_heater_state = futures[water_heater_info.key].result()
        assert isinstance(water_heater_state, WaterHeaterState)
        assert water_heater_state.missing_state is False
        assert water_heater_state.mode is WaterHeaterMode.ECO

        # Its first value is OFF, which is also the default the entity starts on,
        # so it only leaves unknown if the first value publishes regardless
        lambda_state = futures[lambda_water_heater_info.key].result()
        assert isinstance(lambda_state, WaterHeaterState)
        assert lambda_state.missing_state is False
        assert lambda_state.mode is WaterHeaterMode.OFF
