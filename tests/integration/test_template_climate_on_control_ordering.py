"""Integration test: on_control fires before control()/on_state, with the full ClimateCall.

on_control's lambda argument exposes get_mode()/etc. on the *requested* ClimateCall, while the
entity's own .mode field still reflects the state *before* control() applies the change --
proving the firing order is on_control, then control(), then on_state.
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode
import pytest

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, wait_for_state
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

    log_lines: list[str] = []

    def on_log_line(line: str) -> None:
        if "on_control " in line or "on_state " in line:
            log_lines.append(line)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):

        async def wait_for_climate_state(
            timeout: float = 5.0,
        ) -> aioesphomeapi.ClimateState:
            return await wait_for_state(
                client, lambda s: isinstance(s, aioesphomeapi.ClimateState), timeout
            )

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"
        test_climate = climate_infos[0]

        client.subscribe_states(
            initial_state_helper.on_state_wrapper(lambda state: None)
        )
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT

        await asyncio.sleep(0.2)

        # on_control saw the new requested mode (3 == CLIMATE_MODE_HEAT) while the entity's own
        # state was still the old one (0 == CLIMATE_MODE_OFF) -- proving it fired before control().
        assert any(
            "on_control requested_mode=3 current_mode_before_apply=0" in line
            for line in log_lines
        )
        # on_state fired afterward, reporting the now-applied mode.
        assert any("on_state mode=3" in line for line in log_lines)

        control_index = next(
            i for i, line in enumerate(log_lines) if "on_control " in line
        )
        state_index = next(i for i, line in enumerate(log_lines) if "on_state " in line)
        assert control_index < state_index, "on_control must fire before on_state"
