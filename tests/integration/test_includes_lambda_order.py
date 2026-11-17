"""Lambdas rendered at file scope can use the user's includes, which can name ids."""

from __future__ import annotations

from aioesphomeapi import SensorInfo, SensorState
import pytest

from .state_utils import require_entity, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_includes_lambda_order(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The apply action and the sensor filter use the header, the header reads the global."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sensor = require_entity(entities, "header_value", SensorInfo)
        state = await wait_for_state(
            client,
            lambda s: (
                isinstance(s, SensorState)
                and s.key == sensor.key
                and not s.missing_state
            ),
        )
        # globals.set stored HEADER_CONSTANT and the header read it back doubled.
        assert state.state == 84.0
