"""Integration tests for template climate: state-readback lambdas, optimistic correction flow.

The device's lambdas always return fixed values regardless of commands.  This
verifies that with optimistic=true the device's correction (via lambdas) still
takes effect even though commands are temporarily applied as a preview.

Key observable behaviour
------------------------
With optimistic=true:
  1. control() applies the command to the internal state (mode=HEAT).
  2. loop() detects the mismatch (HEAT ≠ lambda's OFF) and publishes the
     corrected state.

The optimistic preview (HEAT) and the correction (OFF) may be coalesced into a
single API message by the batch-deduplication layer — only the final corrected
state is guaranteed to reach external observers.  The test therefore accepts
either one or two updates and verifies the final settled value.

Contrast with optimistic=false: if the action is a no-op, control() does NOT
modify the internal state, so loop() sees no mismatch and publishes nothing.
The observable guarantee of optimistic=true is that *a* state update arrives
and settles at the lambda's value.
"""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode
import pytest

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-lam-corr"


@pytest.mark.asyncio
async def test_template_climate_lambdas_correction(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """State-readback lambdas + optimistic with no-op actions: device rejects every command.

    The fixture's lambdas always return fixed values (mode=OFF, target=22.0°C)
    and the set_*_actions are no-ops.  The test verifies that after each
    command the state settles at the lambda's value, regardless of whether the
    optimistic preview and the correction arrive as one or two API messages.
    """
    clear_host_prefs(DEVICE_NAME)
    async with run_compiled(yaml_config), api_client_connected() as client:

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

        # Lambda always returns OFF / 22.0°C
        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.OFF
        assert initial.target_temperature == pytest.approx(22.0, abs=0.1)

        # --- Mode correction ---
        # control() applies HEAT (optimistic preview); loop() detects the
        # mismatch and corrects to OFF.  The two updates may be coalesced by
        # the API batch layer, so we accept either one or two messages.
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        if state.mode == ClimateMode.HEAT:
            # Preview arrived as a separate message; wait for the correction.
            state = await wait_for_climate_state()
        assert state.mode == ClimateMode.OFF, (
            "Expected device to correct mode back to OFF via lambda"
        )

        # --- Target temperature correction ---
        client.climate_command(test_climate.key, target_temperature=25.0)
        state = await wait_for_climate_state()
        if state.target_temperature == pytest.approx(25.0, abs=0.1):
            # Preview arrived as a separate message; wait for the correction.
            state = await wait_for_climate_state()
        assert state.target_temperature == pytest.approx(22.0, abs=0.1), (
            "Expected device to correct target temperature back to 22.0°C via lambda"
        )
