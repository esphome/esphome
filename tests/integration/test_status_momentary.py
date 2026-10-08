"""Integration test for status_momentary_warning/error without a name."""

from __future__ import annotations

import asyncio

from aioesphomeapi import BinarySensorState, EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_status_momentary(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, services = await client.list_entities_services()
        svc = {s.name: s for s in services}
        keys = {e.object_id: e.key for e in entities}
        warning_key = keys["probe_warning"]
        error_key = keys["probe_error"]

        loop = asyncio.get_running_loop()
        changes: asyncio.Queue[tuple[int, bool, float]] = asyncio.Queue()
        initial: dict[int, bool] = {}

        def on_state(state: EntityState) -> None:
            if not isinstance(state, BinarySensorState):
                return
            if state.key not in initial:
                initial[state.key] = state.state
                return
            changes.put_nowait((state.key, state.state, loop.time()))

        client.subscribe_states(on_state)
        await asyncio.sleep(0.5)
        assert initial.get(warning_key) is False
        assert initial.get(error_key) is False

        async def next_change(key: int) -> tuple[bool, float]:
            while True:
                k, value, when = await asyncio.wait_for(changes.get(), timeout=5)
                if k == key:
                    return value, when

        # The flag sets at once and clears after the length
        start = loop.time()
        await client.execute_service(svc["momentary_warning"], {"length": 300})
        assert (await next_change(warning_key))[0] is True
        value, cleared = await next_change(warning_key)
        assert value is False
        assert cleared - start >= 0.25

        start = loop.time()
        await client.execute_service(svc["momentary_error"], {"length": 300})
        assert (await next_change(error_key))[0] is True
        value, cleared = await next_change(error_key)
        assert value is False
        assert cleared - start >= 0.25

        # A second call restarts the timeout instead of adding a second one
        start = loop.time()
        await client.execute_service(svc["momentary_warning"], {"length": 1000})
        assert (await next_change(warning_key))[0] is True
        await asyncio.sleep(0.6)
        await client.execute_service(svc["momentary_warning"], {"length": 1000})
        value, cleared = await next_change(warning_key)
        assert value is False
        assert cleared - start >= 1.4
