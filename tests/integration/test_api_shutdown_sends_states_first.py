"""A state published right before a shutdown reaches the client before DisconnectRequest."""

from __future__ import annotations

from aioesphomeapi import ButtonInfo, EntityState, SwitchInfo, SwitchState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedWithDisconnectFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_api_shutdown_sends_states_first(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """The switch toggled in the same press as the restart is sent before the disconnect."""
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
