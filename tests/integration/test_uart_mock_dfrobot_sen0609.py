"""Integration test for the DFRobot SEN0609 component against a model of the module's command line.

Checks the boot sequence (settings read back, presence frames parsed) and that a number change
reaches the module as setRange followed by saveConfig.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import BinarySensorInfo, EntityState, NumberInfo, TextSensorInfo
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_uart_mock_dfrobot_sen0609(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Boot reads the module's settings; a number change is written and saved."""
    loop = asyncio.get_running_loop()
    saved = loop.create_future()

    def line_callback(line: str) -> None:
        if "Saved range 0.60 8.00" in line and not saved.done():
            saved.set_result(True)

    states: dict[int, EntityState] = {}

    async def wait_for(condition, what: str) -> None:
        try:
            async with asyncio.timeout(10.0):
                while not condition():
                    await asyncio.sleep(0.05)
        except TimeoutError:
            pytest.fail(f"Timeout waiting for {what}; states: {states}")

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        has_target = require_entity(entities, "has_target", BinarySensorInfo)
        min_range = require_entity(entities, "min_range", NumberInfo)
        max_range = require_entity(entities, "max_range", NumberInfo)
        version = require_entity(entities, "version", TextSensorInfo)

        client.subscribe_states(lambda state: states.__setitem__(state.key, state))

        def state_of(entity):
            state = states.get(entity.key)
            return None if state is None or state.missing_state else state.state

        await wait_for(lambda: state_of(has_target) is True, "presence")
        await wait_for(lambda: state_of(version) == "V1.0.4", "software version")
        await wait_for(
            lambda: state_of(max_range) == pytest.approx(12.0), "maximum range"
        )
        assert state_of(min_range) == pytest.approx(0.6)

        client.number_command(max_range.key, 8.0)
        await wait_or_fail(saved, 10.0, "The new range was not written and saved")
        await wait_for(
            lambda: state_of(max_range) == pytest.approx(8.0), "new maximum range"
        )
