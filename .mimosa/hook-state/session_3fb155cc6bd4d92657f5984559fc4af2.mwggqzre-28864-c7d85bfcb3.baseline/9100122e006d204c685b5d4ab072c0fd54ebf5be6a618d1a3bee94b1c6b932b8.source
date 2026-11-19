"""The merged timeout + throttle_with_priority filter publishes exactly what the two filter chain does."""

from __future__ import annotations

import asyncio
import math

from aioesphomeapi import ButtonInfo, EntityState, SensorState
import pytest

from .state_utils import (
    build_key_to_entity_mapping,
    require_entity,
    subscribe_after_initial_states,
)
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_sensor_timeout_throttle_filter(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    loop = asyncio.get_running_loop()
    values: dict[str, list[float]] = {"merged": [], "reference": []}
    done = {name: loop.create_future() for name in values}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState):
            return
        name = key_to_sensor.get(state.key)
        if name not in values:
            return
        values[name].append(math.nan if state.missing_state else state.state)
        # 99 is published last and republished once by the timeout
        if values[name].count(99.0) == 2 and not done[name].done():
            done[name].set_result(True)

    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        key_to_sensor = build_key_to_entity_mapping(
            entities, {"merged": "Merged", "reference": "Reference"}
        )
        await subscribe_after_initial_states(client, entities, on_state=on_state)

        button = require_entity(entities, "drive", ButtonInfo, exact=True)
        client.button_command(button.key)
        await wait_or_fail(
            asyncio.gather(*done.values()), 5.0, "Timed out", values=values
        )

        merged, reference = values["merged"], values["reference"]
        assert len(merged) == len(reference), (merged, reference)
        for got, want in zip(merged, reference, strict=True):
            assert (math.isnan(got) and math.isnan(want)) or got == want, (
                merged,
                reference,
            )
        assert any(math.isnan(v) for v in merged)
