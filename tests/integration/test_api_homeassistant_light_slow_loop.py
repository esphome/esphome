"""A held report is retired by the transition that replaces it even when that transition ends in
its first loop pass."""

from __future__ import annotations

import asyncio

import pytest

from .state_utils import require_service
from .test_api_homeassistant_light import light_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_homeassistant_light_slow_loop(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The call the replacing transition ends at is sent, not the report held for the old one."""
    async with light_entity(
        yaml_config, run_compiled, api_client_connected, "['color_temp']"
    ) as ha:
        replace_transition = require_service(ha.services, "replace_transition")
        ha.report(None, "on")
        ha.report("color_mode", "color_temp")
        ha.report("brightness", "128")
        ha.report("color_temp_kelvin", "2500")
        await ha.next_state(lambda s: s.state and s.color_temperature == 400.0)

        # A report arrives during a transition and is held for its end
        ha.command(brightness=1.0, transition_length=1.0)
        ha.report("brightness", "50")
        await asyncio.sleep(0.1)  # held before the action runs
        # The action replaces the transition from the scheduler, so the light's loop runs next
        # only after loop_interval, when the new transition is over
        await ha.client.execute_service(replace_transition, {})
        await ha.expect_call(brightness="255", color_temp_kelvin="4000")
        assert ha.calls.empty()
