"""Integration test for light initial_state configuration.

Tests that the initial_state values are correctly applied at boot when
no saved preferences exist. The initial_state callback populates defaults
that the restore logic uses as a fallback.
"""

import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_initial_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that initial_state values are applied at boot."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        light = require_entity(entities, "test_light")

        helper = InitialStateHelper(entities)
        client.subscribe_states(helper.on_state_wrapper(lambda s: None))
        await helper.wait_for_initial_states()

        state = helper.initial_states[light.key]

        # restore_mode: ALWAYS_OFF overrides state to false
        assert state.state is False

        # But the color values from initial_state should be applied
        assert state.brightness == pytest.approx(0.75, abs=0.05)
        assert state.red == pytest.approx(1.0, abs=0.01)
        assert state.green == pytest.approx(0.5, abs=0.01)
        assert state.blue == pytest.approx(0.0, abs=0.01)
