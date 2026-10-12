"""Integration test for status_momentary_warning/error without a name."""

from __future__ import annotations

import asyncio

from aioesphomeapi import BinarySensorState, EntityState
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_status_momentary(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    loop = asyncio.get_running_loop()
    warning_logged = loop.create_future()
    error_logged = loop.create_future()

    def on_log_line(line: str) -> None:
        if (
            "set Warning flag: probe warning reason" in line
            and not warning_logged.done()
        ):
            warning_logged.set_result(True)
        if "set Error flag: probe error reason" in line and not error_logged.done():
            error_logged.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
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

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()
        assert initial_state_helper.initial_states[warning_key].state is False
        assert initial_state_helper.initial_states[error_key].state is False

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
        await asyncio.wait_for(warning_logged, timeout=5)
        await client.execute_service(svc["momentary_error_message"], {})
        await asyncio.wait_for(error_logged, timeout=5)
