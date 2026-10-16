"""Integration test for the DFRobot SEN0465 series over UART against a model of a SEN0466 probe.

Checks the boot sequence (passive mode with one retry, the wait after it, the alarm setting) and that
a reading is decoded and temperature compensated.
"""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

from aioesphomeapi import EntityState, SensorInfo
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

ALARM_RE = re.compile(
    r"Alarm switch (\d+) threshold (\d+) type (\d+), (\d+) ms after the mode change"
)


@pytest.mark.asyncio
async def test_uart_mock_dfrobot_sen0465(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Boot sets passive mode and the alarm before the first reading."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()
    alarm_set: asyncio.Future[re.Match[str]] = loop.create_future()
    lines: list[str] = []

    def line_callback(line: str) -> None:
        lines.append(line)
        if (match := ALARM_RE.search(line)) and not alarm_set.done():
            alarm_set.set_result(match)

    states: dict[int, EntityState] = {}

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        concentration = require_entity(entities, "co", SensorInfo)
        temperature = require_entity(entities, "probe_temperature", SensorInfo)

        client.subscribe_states(lambda state: states.__setitem__(state.key, state))

        def state_of(entity: SensorInfo) -> float | None:
            state = states.get(entity.key)
            return None if state is None or state.missing_state else state.state

        try:
            async with asyncio.timeout(10.0):
                while state_of(concentration) is None:
                    await asyncio.sleep(0.05)
        except TimeoutError:
            pytest.fail(f"No reading; log: {lines[-20:]}")

        match = await asyncio.wait_for(alarm_set, timeout=1.0)
        switch, threshold, alarm_type, delay_ms = (int(x) for x in match.groups())
        assert (switch, threshold, alarm_type) == (1, 35, 1)
        assert delay_ms >= 1000, (
            "the alarm was set before the wait after the mode change"
        )

        assert any(
            "Passive mode failed: not confirmed (attempt 1 of 3)" in x for x in lines
        )
        assert any("Passive mode confirmed" in x for x in lines)
        assert any("Alarm setting confirmed" in x for x in lines)
        assert not any("Data requested before the setup was done" in x for x in lines)

        # 20 ppm at 25 C: 20 / (0.005 * 25 + 0.9) - (0.3 * 25 - 6)
        assert state_of(concentration) == pytest.approx(20 / 1.025 - 1.5, abs=0.05)
        assert state_of(temperature) == pytest.approx(25.0, abs=0.1)
