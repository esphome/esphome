"""Integration test for the DFRobot SEN0609 component against a model of the module's command line.

Checks the boot sequence: the software version is read and presence frames are parsed.
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import BinarySensorInfo, EntityState, TextSensorInfo
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_dfrobot_sen0609(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Boot reads the software version; presence frames reach the binary sensor."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    states: dict[int, EntityState] = {}

    async def wait_for(condition, what: str) -> None:
        try:
            async with asyncio.timeout(10.0):
                while not condition():
                    await asyncio.sleep(0.05)
        except TimeoutError:
            pytest.fail(f"Timeout waiting for {what}; states: {states}")

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        has_target = require_entity(entities, "has_target", BinarySensorInfo)
        version = require_entity(entities, "version", TextSensorInfo)

        client.subscribe_states(lambda state: states.__setitem__(state.key, state))

        def state_of(entity):
            state = states.get(entity.key)
            return None if state is None or state.missing_state else state.state

        await wait_for(lambda: state_of(has_target) is True, "presence")
        await wait_for(lambda: state_of(version) == "V1.0.4", "software version")
