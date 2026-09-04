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
from .state_utils import InitialStateHelper, require_entity, wait_for_state
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

    log_lines: list[str] = []

    def on_log_line(line: str) -> None:
        if "on_control " in line:
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

        client.subscribe_states(
            initial_state_helper.on_state_wrapper(lambda state: None)
        )
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.current_temperature == pytest.approx(22.5, abs=0.1)
        assert initial.current_humidity == pytest.approx(55.0, abs=0.1)
        assert initial.action == ClimateAction.IDLE
        assert initial.mode == ClimateMode.OFF
        # Nothing was commanded yet: on_control must not have fired.
        assert not log_lines

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
        assert any(
            "on_control mode=3" in line for line in log_lines
        )  # CLIMATE_MODE_HEAT
        assert any("on_control target_temperature=22.5" in line for line in log_lines)
        assert any("on_control fan_mode=" in line for line in log_lines)
        assert any("on_control swing_mode=" in line for line in log_lines)
        assert any("on_control preset=" in line for line in log_lines)
        # Exactly one on_control log line per command, none extra (e.g. from a stray republish).
        assert len(log_lines) == 5

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
