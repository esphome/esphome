"""Integration test: each settable field forwards its value to the matching set_*_action.

With optimistic: false the entity state stays put until climate.template.publish reports the
device's actual state back, so the actions are the only thing that reacts to a command.
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import (
    ButtonInfo,
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
)
import pytest

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, require_entity, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-set-act"


@pytest.mark.asyncio
async def test_template_climate_set_actions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Every set_*_action fires with the requested value; state waits for a publish."""
    clear_host_prefs(DEVICE_NAME)

    log_lines: list[str] = []

    def on_log_line(line: str) -> None:
        if "_action " in line or "Unsupported" in line:
            log_lines.append(line)

    def logged(fragment: str) -> bool:
        return any(fragment in line for line in log_lines)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"
        test_climate = climate_infos[0]

        report_button = require_entity(entities, "report_device_state", ButtonInfo)
        unsupported_button = require_entity(
            entities, "report_unsupported_mode", ButtonInfo
        )

        client.subscribe_states(
            initial_state_helper.on_state_wrapper(lambda state: None)
        )
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Both traits are derived from the low/high and humidity set actions, not declared.
        assert test_climate.supports_two_point_target_temperature
        assert test_climate.supports_target_humidity

        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        client.climate_command(
            test_climate.key, target_temperature_low=18.0, target_temperature_high=24.0
        )
        client.climate_command(test_climate.key, target_humidity=55)
        client.climate_command(test_climate.key, fan_mode=ClimateFanMode.LOW)
        client.climate_command(test_climate.key, custom_fan_mode="turbo")
        client.climate_command(test_climate.key, swing_mode=ClimateSwingMode.VERTICAL)
        client.climate_command(test_climate.key, preset=ClimatePreset.ECO)
        client.climate_command(test_climate.key, custom_preset="eco_plus")

        for _ in range(50):
            await asyncio.sleep(0.1)
            if logged("set_custom_preset_action eco_plus"):
                break

        assert logged("set_mode_action 3")  # CLIMATE_MODE_HEAT
        assert logged("set_target_temperature_low_action 18.0")
        assert logged("set_target_temperature_high_action 24.0")
        assert logged("set_target_humidity_action 55")
        assert logged("set_fan_mode_action 3")  # CLIMATE_FAN_LOW
        assert logged("set_custom_fan_mode_action turbo")
        assert logged("set_swing_mode_action 2")  # CLIMATE_SWING_VERTICAL
        assert logged("set_preset_action 5")  # CLIMATE_PRESET_ECO
        assert logged("set_custom_preset_action eco_plus")

        # optimistic: false, so none of the commands above touched the entity's own state --
        # a device report is what actually moves it.
        client.button_command(report_button.key)
        state = await wait_for_state(
            client, lambda s: isinstance(s, aioesphomeapi.ClimateState)
        )
        assert state.mode == ClimateMode.HEAT

        # A publish naming a mode outside supported_modes warns instead of publishing it.
        client.button_command(unsupported_button.key)
        for _ in range(50):
            await asyncio.sleep(0.1)
            if logged("Unsupported mode"):
                break
        assert logged("Unsupported mode")
