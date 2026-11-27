"""States published right before a shutdown reach the client before DisconnectRequest."""

from __future__ import annotations

from aioesphomeapi import (
    ButtonInfo,
    EntityState,
    SensorInfo,
    SensorState,
    SwitchInfo,
    SwitchState,
)
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedWithDisconnectFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# Sensors in the fixture: more than MAX_MESSAGES_PER_BATCH (34) states
BURST_SENSORS = 40


@pytest.mark.asyncio
@pytest.mark.shared_yaml("api_shutdown_sends_states_first")
async def test_api_shutdown_sends_states_first(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    async with (
        run_compiled(yaml_config),
        api_client_connected_with_disconnect() as (client, disconnect_event),
    ):
        entities, _ = await client.list_entities_services()
        switch = require_entity(entities, "test_switch", SwitchInfo)
        button = require_entity(entities, "toggle_then_restart", ButtonInfo)

        states: list[EntityState] = []
        sub = await subscribe_after_initial_states(client, entities, states.append)
        assert sub.initial_states[switch.key].state is False

        client.button_command(button.key)
        await wait_or_fail(
            disconnect_event, msg="device never disconnected", states=states
        )

        assert [s.state for s in states if isinstance(s, SwitchState)] == [True], (
            "the toggled state must arrive before the device disconnects"
        )


@pytest.mark.asyncio
@pytest.mark.shared_yaml("api_shutdown_sends_states_first")
async def test_api_shutdown_sends_burst_before_disconnect(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """More states than one batch packet holds all arrive before the disconnect."""
    async with (
        run_compiled(yaml_config),
        api_client_connected_with_disconnect() as (client, disconnect_event),
    ):
        entities, _ = await client.list_entities_services()
        button = require_entity(entities, "publish_then_restart", ButtonInfo)
        burst_keys = {e.key for e in entities if isinstance(e, SensorInfo)}
        assert len(burst_keys) == BURST_SENSORS

        states: list[EntityState] = []
        await subscribe_after_initial_states(client, entities, states.append)

        client.button_command(button.key)
        await wait_or_fail(
            disconnect_event, msg="device never disconnected", states=states
        )

        received = {
            s.key for s in states if isinstance(s, SensorState) and s.key in burst_keys
        }
        assert received == burst_keys, (
            f"{len(burst_keys - received)} of {BURST_SENSORS} states missed the disconnect"
        )
