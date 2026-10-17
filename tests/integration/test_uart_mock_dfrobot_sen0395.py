"""Integration test for the DFRobot SEN0395 component against a model of the module's command line.

Checks presence frames, recovery after an overlong line, a command answered by "Done" and the
reset command, which the module answers only with its "leapMMW:/>" prompt (no line end).
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import BinarySensorInfo, ButtonInfo, EntityState, SwitchInfo
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_dfrobot_sen0395(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Frames parse, an overlong line is dropped and the prompt completes the reset."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()
    dropped = loop.create_future()
    restarted = loop.create_future()

    def line_callback(line: str) -> None:
        if "Line over 252 chars dropped" in line and not dropped.done():
            dropped.set_result(True)
        if "Restarted sensor." in line and not restarted.done():
            restarted.set_result(True)

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
        detected = require_entity(entities, "detected", BinarySensorInfo)
        active = require_entity(entities, "sensor_active", SwitchInfo)
        present = require_entity(entities, "inject_present", ButtonInfo)
        overlong = require_entity(entities, "inject_overlong_then_absent", ButtonInfo)
        reset = require_entity(entities, "reset_sensor", ButtonInfo)

        client.subscribe_states(lambda state: states.__setitem__(state.key, state))

        def state_of(entity):
            state = states.get(entity.key)
            return None if state is None or state.missing_state else state.state

        client.button_command(present.key)
        await wait_for(lambda: state_of(detected) is True, "presence")

        client.button_command(overlong.key)
        await wait_for(
            lambda: state_of(detected) is False, "absence after overlong line"
        )
        await asyncio.wait_for(dropped, timeout=5.0)

        client.switch_command(active.key, False)
        await wait_for(lambda: state_of(active) is False, "sensor stopped")

        client.button_command(reset.key)
        try:
            await asyncio.wait_for(restarted, timeout=10.0)
        except TimeoutError:
            pytest.fail("The reset was not completed by the prompt")
