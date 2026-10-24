"""Integration test for status_momentary_warning/error without a name."""

from __future__ import annotations

import asyncio

from aioesphomeapi import BinarySensorState, EntityState
import pytest

from .log_utils import LineWaiter
from .state_utils import subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_status_momentary(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    loop = asyncio.get_running_loop()
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, services = await client.list_entities_services()
        svc = {s.name: s for s in services}
        keys = {e.object_id: e.key for e in entities}
        warning_key = keys["probe_warning"]
        error_key = keys["probe_error"]

        changes: asyncio.Queue[tuple[int, bool, float]] = asyncio.Queue()

        def on_state(state: EntityState) -> None:
            if isinstance(state, BinarySensorState):
                changes.put_nowait((state.key, state.state, loop.time()))

        sub = await subscribe_after_initial_states(client, entities, on_state=on_state)
        assert sub.initial_states[warning_key].state is False
        assert sub.initial_states[error_key].state is False

        async def next_change(key: int) -> tuple[bool, float]:
            while True:
                k, value, when = await asyncio.wait_for(changes.get(), timeout=5)
                if k == key:
                    return value, when

        # The flag sets at once and clears after the length
        for service, key in (
            ("momentary_warning", warning_key),
            ("momentary_error", error_key),
        ):
            start = loop.time()
            await client.execute_service(svc[service], {"length": 300})
            assert (await next_change(key))[0] is True
            value, cleared = await next_change(key)
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

        # A message replaces "unspecified" in the log line
        await client.execute_service(svc["momentary_warning_message"], {})
        await waiter.wait_for("set Warning flag: probe warning reason", timeout=5)
        await client.execute_service(svc["momentary_error_message"], {})
        await waiter.wait_for("set Error flag: probe error reason", timeout=5)
