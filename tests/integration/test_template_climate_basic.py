"""Integration test for template climate: sensor-pushed measured values, on_control + publish
for the settable ones.

current_temperature/current_humidity are pushed by a referenced sensor/humidity_sensor (no
polling); action is set once at boot via climate.template.publish, since it has no sensor
equivalent. mode/target_temperature/fan_mode/swing_mode/preset are plain internal state:
on_control fires exactly once per command (never before the first one), and
climate.template.publish simulates the device reporting its own state independent of any prior
command -- that report is authoritative, overriding whatever was optimistically applied earlier.
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import (
    ButtonInfo,
    ClimateAction,
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
)
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

DEVICE_NAME = "tmpl-clim-basic"


@pytest.mark.asyncio
async def test_template_climate_basic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Sensor-pushed measured values, on_control + publish for settable ones."""
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

        # Advertised capabilities come straight from the supported_*/custom_* config lists.
        assert ClimateMode.OFF in test_climate.supported_modes
        assert ClimateMode.HEAT in test_climate.supported_modes
        assert ClimateMode.COOL in test_climate.supported_modes

        assert ClimateFanMode.AUTO in test_climate.supported_fan_modes
        assert ClimateFanMode.LOW in test_climate.supported_fan_modes
        assert ClimateFanMode.HIGH in test_climate.supported_fan_modes

        assert ClimateSwingMode.OFF in test_climate.supported_swing_modes
        assert ClimateSwingMode.VERTICAL in test_climate.supported_swing_modes

        assert ClimatePreset.NONE in test_climate.supported_presets
        assert ClimatePreset.ECO in test_climate.supported_presets

        report_button = require_entity(entities, "simulate_device_report", ButtonInfo)

        sub = await subscribe_after_initial_states(client, entities)

        initial = sub.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.current_temperature == pytest.approx(22.5, abs=0.1)
        assert initial.current_humidity == pytest.approx(55.0, abs=0.1)
        assert initial.action == ClimateAction.IDLE
        assert initial.mode == ClimateMode.OFF
        # Nothing was commanded yet: on_control must not have fired.
        assert not waiter.count("on_control ")

        # Commands apply optimistically and on_control fires with the same values.
        client.climate_command(test_climate.key, mode=ClimateMode.HEAT)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT

        client.climate_command(test_climate.key, target_temperature=22.5)
        state = await wait_for_climate_state()
        assert state.target_temperature == pytest.approx(22.5, abs=0.1)

        client.climate_command(test_climate.key, fan_mode=ClimateFanMode.HIGH)
        state = await wait_for_climate_state()
        assert state.fan_mode == ClimateFanMode.HIGH

        client.climate_command(test_climate.key, swing_mode=ClimateSwingMode.VERTICAL)
        state = await wait_for_climate_state()
        assert state.swing_mode == ClimateSwingMode.VERTICAL

        client.climate_command(test_climate.key, preset=ClimatePreset.ECO)
        state = await wait_for_climate_state()
        assert state.preset == ClimatePreset.ECO

        await asyncio.sleep(0.2)
        assert waiter.count("on_control mode=3")  # CLIMATE_MODE_HEAT
        assert waiter.count("on_control target_temperature=22.5")
        assert waiter.count("on_control fan_mode=")
        assert waiter.count("on_control swing_mode=")
        assert waiter.count("on_control preset=")
        # Exactly one on_control log line per command, none extra (e.g. from a stray republish).
        assert waiter.count("on_control ") == 5

        # measured values are untouched by any of the above (no set action exists for them).
        assert state.current_temperature == pytest.approx(22.5, abs=0.1)
        assert state.current_humidity == pytest.approx(55.0, abs=0.1)
        assert state.action == ClimateAction.IDLE

        # The device's report is authoritative and overrides everything commanded above.
        client.button_command(report_button.key)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.OFF
        assert state.fan_mode == ClimateFanMode.AUTO
        assert state.swing_mode == ClimateSwingMode.OFF
        assert state.preset == ClimatePreset.NONE
