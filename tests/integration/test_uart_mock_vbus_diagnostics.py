"""Real parser errors and scan status are observable without interrupting retained RX data."""

import asyncio

from aioesphomeapi import ButtonInfo, SensorInfo, SensorState
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
@pytest.mark.shared_yaml("uart_mock_vbus_setting_write")
async def test_uart_mock_vbus_diagnostics(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    complete = asyncio.Event()
    error = asyncio.Event()
    states: dict[int, SensorState] = {}
    observed_statuses: list[float] = []
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        corrupt = find_entity(entities, "inject_corrupt_packet", ButtonInfo)
        counter = find_entity(entities, "packet_errors", SensorInfo)
        status = find_entity(entities, "scan_status", SensorInfo)
        rx = find_entity(entities, "controller_rx", SensorInfo)
        assert start is not None and corrupt is not None and counter is not None
        assert status is not None and rx is not None

        def on_state(state: SensorState) -> None:
            if not isinstance(state, SensorState):
                return
            states[state.key] = state
            if state.key == status.key and not state.missing_state:
                observed_statuses.append(state.state)
                if state.state == 2:
                    complete.set()
            if (
                state.key == counter.key
                and not state.missing_state
                and state.state == 1
            ):
                error.set()

        client.subscribe_states(on_state)
        client.button_command(start.key)
        await asyncio.wait_for(complete.wait(), timeout=12)
        assert 1 in observed_statuses
        assert states[counter.key].state == 0
        client.button_command(corrupt.key)
        await asyncio.wait_for(error.wait(), timeout=5)
        assert states[counter.key].state == 1
        assert states[status.key].state == 2
        assert states[rx.key].state == pytest.approx(12.3)
        assert not states[rx.key].missing_state
