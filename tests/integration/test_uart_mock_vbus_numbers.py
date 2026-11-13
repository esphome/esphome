"""Numeric API controls expose actual limits and keep confirmed states through writes and no-ops."""

import asyncio
import re

from aioesphomeapi import ButtonInfo, NumberInfo, NumberState
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
@pytest.mark.shared_yaml("uart_mock_vbus_setting_write")
async def test_uart_mock_vbus_numbers(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    states: dict[int, NumberState] = {}
    values: list[float] = []
    initial = asyncio.Event()
    verified = asyncio.Event()
    unchanged = asyncio.Event()
    states_at_write: list[float] = []
    missing_after_confirmation: list[bool] = []
    minimum_key = 0

    def on_line(line: str) -> None:
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        if "[vbus.wire_test:" in line:
            frame = bytes.fromhex(text.removesuffix(" (16)").replace(".", " "))
            if frame[7] == 2:
                states_at_write.append(states[minimum_key].state)
        if (
            "[vbus.write_test:" in line
            and [int(value) for value in text.split()][1] == 4
        ):
            unchanged.set()

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        minimum = find_entity(entities, "minimum_pump_speed", NumberInfo)
        delta = find_entity(entities, "switch_on_temperature_difference", NumberInfo)
        assert start is not None and minimum is not None and delta is not None
        minimum_key = minimum.key

        def on_state(state: NumberState) -> None:
            if not isinstance(state, NumberState):
                return
            states[state.key] = state
            if state.key == minimum.key and values:
                missing_after_confirmation.append(state.missing_state)
            if state.key == minimum.key and not state.missing_state:
                values.append(state.state)
                if state.state == 35:
                    initial.set()
                if state.state == 60:
                    verified.set()

        client.subscribe_states(on_state)
        client.button_command(start.key)
        await asyncio.wait_for(initial.wait(), timeout=12)
        assert values == [35]
        assert states_at_write == []
        client.number_command(minimum.key, 60)
        assert states[minimum.key].state == 35
        await asyncio.wait_for(verified.wait(), timeout=5)
        assert values == [35, 60]
        assert states_at_write == [35]
        client.number_command(minimum.key, 60)
        await asyncio.wait_for(unchanged.wait(), timeout=5)
        await asyncio.sleep(0.1)
        assert values == [35, 60]
        assert states_at_write == [35]
        assert not states[minimum.key].missing_state
        assert not any(missing_after_confirmation)

        # Explicit metadata listing resends current states; keep it outside the duplicate-state assertions.
        entities, _ = await client.list_entities_services()
        live_minimum = find_entity(entities, "minimum_pump_speed", NumberInfo)
        live_delta = find_entity(
            entities, "switch_on_temperature_difference", NumberInfo
        )
        assert live_minimum is not None and live_delta is not None
        assert (live_minimum.min_value, live_minimum.max_value, live_minimum.step) == (
            10,
            60,
            5,
        )
        assert (live_delta.min_value, live_delta.max_value, live_delta.step) == (
            4.5,
            11.5,
            0.5,
        )
        assert live_delta.unit_of_measurement == "K"
