"""select.set with a constant option and a template select lambda, both set by index."""

from __future__ import annotations

from aioesphomeapi import SelectInfo, SelectState
import pytest

from .log_utils import LineWaiter
from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_select_set_constant(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    waiter_log = LineWaiter()
    invalid_lambda = waiter_log.expect("'Picked': Invalid option turbo")
    async with (
        run_compiled(yaml_config, line_callback=waiter_log.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        fan = require_entity(entities, "fan", SelectInfo, exact=True)
        picked = require_entity(entities, "picked", SelectInfo, exact=True)
        sub = await subscribe_after_initial_states(client, entities)

        async def press(button: str, key: int) -> str:
            state = await press_and_wait(
                client, entities, sub.waiter, button, SelectState, key=key
            )
            return state.state

        assert await press("set_high", fan.key) == "high"
        # An option the select does not have changes nothing; the next press is the next state
        client.button_command(require_entity(entities, "set_invalid").key)
        assert await press("set_medium", fan.key) == "medium"

        assert await press("pick_high", picked.key) == "high"
        client.button_command(require_entity(entities, "pick_invalid").key)
        await wait_or_fail(invalid_lambda, 5.0, "invalid lambda option was not logged")
        assert await press("pick_medium", picked.key) == "medium"
