"""Integration test for the template time platform."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, TextSensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction

# Timestamps are exposed as text sensors (rather than 32-bit float sensor states,
# which cannot represent a UNIX epoch exactly) so the exact value can be checked.
EXPECTED_STATES = {
    "fixed_timestamp": "1700000000",
    "offset_timestamp": "1700003600",
    "empty_time_valid": "invalid",
}


@pytest.mark.asyncio
async def test_template_time(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Verify the template time platform evaluates its lambda on demand."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        key_to_name = build_key_to_entity_mapping(entities, list(EXPECTED_STATES))
        events = {name: asyncio.Event() for name in EXPECTED_STATES}

        def on_state(state: EntityState) -> None:
            if isinstance(state, TextSensorState) and not state.missing_state:
                name = key_to_name.get(state.key)
                if name is not None and state.state == EXPECTED_STATES[name]:
                    events[name].set()

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        for name, event in events.items():
            try:
                await asyncio.wait_for(event.wait(), timeout=3.0)
            except TimeoutError:
                pytest.fail(f"Timeout waiting for {name} to report its expected value")
