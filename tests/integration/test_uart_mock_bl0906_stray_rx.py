"""A BL0906 that leaves a stray byte in the receive buffer keeps reading."""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import EntityState, SensorInfo
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_bl0906_stray_rx(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A stray received byte does not stop the read cycle."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    lines: list[str] = []
    states: dict[int, EntityState] = {}

    async with (
        run_compiled(yaml_config, line_callback=lines.append),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        temperature = require_entity(entities, "temperature", SensorInfo)
        voltage = require_entity(entities, "voltage", SensorInfo)

        client.subscribe_states(lambda state: states.__setitem__(state.key, state))

        def state_of(entity: SensorInfo) -> float | None:
            state = states.get(entity.key)
            return None if state is None or state.missing_state else state.state

        try:
            async with asyncio.timeout(10.0):
                while state_of(voltage) is None:
                    await asyncio.sleep(0.05)
        except TimeoutError:
            pytest.fail(f"No voltage reading; log: {lines[-20:]}")

        assert any("RX inject 5 bytes" in x for x in lines), "stray byte not sent"
        assert state_of(temperature) == pytest.approx(10.0, abs=0.01)
        uref = 1.097 * 100000 / (13162 * 100 * 1000)
        assert state_of(voltage) == pytest.approx(0x2A1B3C * uref, rel=1e-5)
        assert not any("Junk on wire" in x for x in lines)
