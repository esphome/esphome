"""Integration test: on_control fires before control()/on_state, with the full ClimateCall.

on_control's lambda argument exposes get_mode()/etc. on the *requested* ClimateCall, while the
entity's own .mode field still reflects the state *before* control() applies the change --
proving the firing order is on_control, then control(), then on_state.
"""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode
import pytest

from .host_prefs import clear_host_prefs
from .log_utils import LineWaiter
from .state_utils import (
    require_only_entity,
    subscribe_after_initial_states,
    wait_for_state,
)
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-oc-order"


@pytest.mark.asyncio
async def test_template_climate_on_control_ordering(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """on_control sees the requested value while the entity's own state is still the old one."""
    clear_host_prefs(DEVICE_NAME)

    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):

        async def wait_for_climate_state(
            timeout: float = 5.0,
        ) -> aioesphomeapi.ClimateState:
            return await wait_for_state(
                client, lambda s: isinstance(s, aioesphomeapi.ClimateState), timeout
            )

        entities, _ = await client.list_entities_services()
        test_climate = require_only_entity(entities, ClimateInfo)

        await subscribe_after_initial_states(client, entities)

        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT

        # on_control saw the new requested mode (3 == CLIMATE_MODE_HEAT) while the entity's own
        # state was still the old one (0 == CLIMATE_MODE_OFF) -- proving it fired before control().
        await waiter.wait_for(
            "on_control requested_mode=3 current_mode_before_apply=0", timeout=5.0
        )
        # on_state fired afterward, reporting the now-applied mode.
        await waiter.wait_for("on_state mode=3", timeout=5.0)

        lines = waiter.lines
        control_index = next(i for i, line in enumerate(lines) if "on_control " in line)
        state_index = next(i for i, line in enumerate(lines) if "on_state " in line)
        assert control_index < state_index, "on_control must fire before on_state"
