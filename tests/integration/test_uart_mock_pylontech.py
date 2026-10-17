"""Integration test for the pylontech component with mock UART.

Phases, all injected after the Start Scenario button press:
  1. Older firmware header and two battery lines publish exact values
  2. Garbage bytes are skipped
  3. A truncated battery line publishes nothing and is counted in a warning
  4. A line over the 199 character limit is dropped with a warning
  5. Recovery: a newer firmware header, itself over the limit, still selects the
     layout with the extra Id columns, so its battery lines parse correctly
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

SENSORS = [
    "bat1_voltage",
    "bat1_current",
    "bat1_temperature",
    "bat1_temperature_low",
    "bat1_temperature_high",
    "bat1_voltage_low",
    "bat1_voltage_high",
    "bat1_coulomb",
    "bat1_mos_temperature",
    "bat2_voltage",
    "bat2_current",
    "bat2_temperature_high",
    "bat2_coulomb",
]
TEXT_SENSORS = ["bat1_base_state", "bat2_base_state"]

# Values per entity: phase 1 reply, then the phase 5 recovery reply
EXPECTED = {
    "bat1_voltage": [50.548, 49.405],
    "bat1_current": [8.91, 0.0],
    "bat1_temperature": [25.0, 17.6],
    "bat1_temperature_low": [24.2, 13.7],
    "bat1_temperature_high": [25.0, 14.5],
    "bat1_voltage_low": [3.368, 3.293],
    "bat1_voltage_high": [3.371, 3.294],
    "bat1_coulomb": [97.0, 60.0],
    "bat1_mos_temperature": [22.7, 16.6],
    "bat2_voltage": [46.012, 48.1],
    "bat2_current": [-1.255, -2.5],
    "bat2_temperature_high": [5.5, 14.1],
    "bat2_coulomb": [4.0, 42.0],
}
EXPECTED_TEXT = {
    "bat1_base_state": ["Charge", "Idle"],
    "bat2_base_state": ["SysError", "Dischg"],
}


@pytest.mark.asyncio
async def test_uart_mock_pylontech(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test pylontech parsing with happy path, garbage, overflow and recovery."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    overflow_lines: list[str] = []
    rejected_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Line over 199 chars dropped" in line:
            overflow_lines.append(line)
        if "malformed battery lines" in line:
            rejected_lines.append(line)

    collector = SensorStateCollector(
        sensor_names=SENSORS, text_sensor_names=TEXT_SENSORS
    )
    recovery_received = collector.add_waiter(
        lambda: (
            all(len(collector.sensor_states[name]) >= 2 for name in SENSORS)
            and all(
                len(collector.text_sensor_states[name]) >= 2 for name in TEXT_SENSORS
            )
        )
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(recovery_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for the recovery reply. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  text_states: {collector.text_sensor_states}"
            )
        # Let any stray publish from the bad phases arrive before the exact checks
        await asyncio.sleep(0.2)

        for name, values in EXPECTED.items():
            assert collector.sensor_states[name] == pytest.approx(values), name
        for name, values in EXPECTED_TEXT.items():
            assert collector.text_sensor_states[name] == values, name

        # Phase 4 and the phase 5 header both overflow
        assert len(overflow_lines) == 2, overflow_lines
        # The phase 3 cut line is counted and reported once the UART is idle
        assert rejected_lines, "no rejected-line summary logged"
