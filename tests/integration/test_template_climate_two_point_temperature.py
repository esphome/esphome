"""Integration tests for template climate: two-point target temperature + humidity.

Covers the supports_two_point_target_temperature/supports_target_humidity boolean flags plus
on_control (forwarding commands out) and climate.template.publish (the device reporting its own
authoritative state, independent of any prior command -- e.g. a device that owns its own setpoint,
changed via a physical remote).
"""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ButtonInfo, ClimateInfo, ClimateMode
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

DEVICE_NAME = "tmpl-clim-two-point"


@pytest.mark.asyncio
async def test_template_climate_two_point_temperature(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Two-point target temperature + humidity: booleans, on_control, and publish precedence."""
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
        assert test_climate.name == "Test Two-Point Heatpump"
        assert test_climate.supports_two_point_target_temperature
        assert test_climate.supports_target_humidity

        report_button = require_entity(entities, "simulate_device_report", ButtonInfo)

        sub = await subscribe_after_initial_states(client, entities)

        initial = sub.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        # Nothing has been published yet: settable fields have no sensor to seed them from, so
        # the entity starts at ESPHome's plain defaults. current_temperature is pushed by the
        # referenced sensor, which has already settled by the time we get here.
        assert initial.mode == ClimateMode.OFF
        assert initial.current_temperature == pytest.approx(21.0, abs=0.1)

        # The device reports its actual state for the first time.
        client.button_command(report_button.key)
        state = await wait_for_climate_state()
        assert state.mode == ClimateMode.HEAT_COOL
        assert state.target_temperature_low == pytest.approx(18.0, abs=0.1)
        assert state.target_temperature_high == pytest.approx(24.0, abs=0.1)
        assert state.target_humidity == pytest.approx(50.0, abs=0.1)

        # Commands apply optimistically (settable fields are plain internal state), and on_control
        # fires with the same values so a real config could forward them to the device.
        client.climate_command(
            test_climate.key, target_temperature_low=19.0, target_temperature_high=25.0
        )
        state = await wait_for_climate_state()
        assert state.target_temperature_low == pytest.approx(19.0, abs=0.1)
        assert state.target_temperature_high == pytest.approx(25.0, abs=0.1)
        await waiter.wait_for_each(
            "on_control target_temperature_low=19.0",
            "on_control target_temperature_high=25.0",
            timeout=5.0,
        )

        client.climate_command(test_climate.key, target_humidity=45.0)
        state = await wait_for_climate_state()
        assert state.target_humidity == pytest.approx(45.0, abs=0.1)
        await waiter.wait_for("on_control target_humidity=45.0", timeout=5.0)

        # The device's next report is authoritative and overrides whatever was optimistically
        # applied above -- this is the whole point of climate.template.publish: a device that owns
        # its own state (e.g. changed by a physical remote) always wins.
        client.button_command(report_button.key)
        state = await wait_for_climate_state()
        assert state.target_temperature_low == pytest.approx(18.0, abs=0.1)
        assert state.target_temperature_high == pytest.approx(24.0, abs=0.1)
        assert state.target_humidity == pytest.approx(50.0, abs=0.1)
