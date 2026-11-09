"""A while whose body never waits ends without a crash or hang; it yields only without arguments."""

from __future__ import annotations

import asyncio

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_while_without_wait(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    waiter = LineWaiter()

    def index(text: str) -> int:
        return next(i for i, line in enumerate(waiter.lines) if text in line)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()

        # The condition only turns false when the main loop runs
        await client.execute_service(require_service(services, "run_entity_loops"), {})
        await waiter.wait_count("INLINE_NOARG done", 1, 5.0, "inline loop did not end")
        await waiter.wait_count("OBJECT_NOARG done", 1, 5.0, "object loop did not end")

        # A busy loop yields after the first slow pass, unless it has arguments
        await client.execute_service(require_service(services, "run_busy_loops"), {})
        await waiter.wait_count(
            "BUSY_OBJECT_STR caller", 1, 5.0, "busy loops did not end"
        )
        for name in ("BUSY_INLINE_NOARG", "BUSY_OBJECT_NOARG"):
            assert index(f"{name} caller") < index(f"{name} done"), name
        for name in ("BUSY_INLINE_STR", "BUSY_OBJECT_STR"):
            assert index(f"{name} done") < index(f"{name} caller"), name

        # A stop while the loop waits for its next turn ends it for good
        done_before = waiter.count("NOARG done")
        await client.execute_service(
            require_service(services, "run_stop_while_yielded"), {}
        )
        await waiter.wait_count("STOP running 0 0", 1, 5.0, "stopped loops still run")
        await asyncio.sleep(0.2)
        assert waiter.count("NOARG done") == done_before

        # Still answering after every loop
        await client.device_info()
