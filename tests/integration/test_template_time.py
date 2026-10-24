"""Integration test for the template time platform."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, TextSensorState
import pytest

from .state_utils import build_key_to_entity_mapping, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

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

        await subscribe_after_initial_states(client, entities, on_state=on_state)

        for name, event in events.items():
            await wait_or_fail(
                event, 3.0, f"Timeout waiting for {name} to report its expected value"
            )
