"""Integration test for template climate: custom fan modes and presets.

Same on_control (forward) + climate.template.publish (device report, authoritative) pattern as
the enum-based mode/preset fields, but for the custom string variants.
"""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ButtonInfo, ClimateInfo
import pytest

from .host_prefs import clear_host_prefs
from .log_utils import LineWaiter
from .state_utils import (
    require_entity,
    require_only_entity,
    subscribe_after_initial_states,
    wait_for_state,
)
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-custom"


@pytest.mark.asyncio
async def test_template_climate_custom_modes(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Custom fan mode/preset: traits, on_control forwarding, and publish precedence."""
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

        assert set(test_climate.supported_custom_fan_modes) == {
            "turbo",
            "silent",
            "eco",
        }
        assert set(test_climate.supported_custom_presets) == {
            "eco_plus",
            "power_save",
            "max",
        }

        report_button = require_entity(entities, "simulate_device_report", ButtonInfo)

        sub = await subscribe_after_initial_states(client, entities)

        initial = sub.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.custom_fan_mode == ""
        assert initial.custom_preset == ""

        client.climate_command(test_climate.key, custom_fan_mode="turbo")
        state = await wait_for_climate_state()
        assert state.custom_fan_mode == "turbo"

        client.climate_command(test_climate.key, custom_preset="power_save")
        state = await wait_for_climate_state()
        assert state.custom_preset == "power_save"

        await waiter.wait_for_each(
            "on_control custom_fan_mode=turbo",
            "on_control custom_preset=power_save",
            timeout=5.0,
        )

        # The device's report is authoritative and overrides what was commanded above.
        client.button_command(report_button.key)
        state = await wait_for_climate_state()
        assert state.custom_fan_mode == "eco"
        assert state.custom_preset == "max"
