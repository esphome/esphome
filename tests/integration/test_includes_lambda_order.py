"""Lambdas rendered at file scope can use the user's includes, which can name ids."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    ButtonInfo,
    EntityState,
    HomeassistantServiceCall,
    SensorInfo,
    SensorState,
)
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_includes_lambda_order(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The apply action, the sensor filter and the event field all use the header."""
    loop = asyncio.get_running_loop()
    sensor_future: asyncio.Future[float] = loop.create_future()
    event_future: asyncio.Future[HomeassistantServiceCall] = loop.create_future()

    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sensor = next(e for e in entities if isinstance(e, SensorInfo))
        button = next(e for e in entities if isinstance(e, ButtonInfo))

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, SensorState)
                and state.key == sensor.key
                and not state.missing_state
                and not sensor_future.done()
            ):
                sensor_future.set_result(state.state)

        def on_service_call(call: HomeassistantServiceCall) -> None:
            if call.is_event and not event_future.done():
                event_future.set_result(call)

        client.subscribe_states(on_state)
        client.subscribe_service_calls(on_service_call)

        # globals.set stored HEADER_CONSTANT and the header read it back doubled.
        assert await asyncio.wait_for(sensor_future, timeout=5.0) == 84.0

        client.button_command(button.key)
        event = await asyncio.wait_for(event_future, timeout=5.0)
        assert event.service == "esphome.header"
        assert event.data == {"text": "from-header"}
