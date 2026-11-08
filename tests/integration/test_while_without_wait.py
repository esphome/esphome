"""Every shape of a while loop whose body never waits ends without a crash or hang.

Without trigger arguments the loop gives the main loop a turn, so a condition the main loop
changes can end it. With arguments it never yields, as they may only be valid while the
trigger runs.
"""

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

        # A busy loop without arguments yields after the first slow pass; with arguments it does not
        await client.execute_service(require_service(services, "run_busy_loops"), {})
        await waiter.wait_count(
            "WAITING_ARG done 3", 1, 5.0, "waiting loop did not end"
        )
        await waiter.wait_count("EMPTY_BODY done", 1, 5.0, "empty loop did not end")
        assert index("BUSY_INLINE_NOARG caller") < index("BUSY_INLINE_NOARG done")
        assert index("BUSY_OBJECT_NOARG caller") < index("BUSY_OBJECT_NOARG done")
        assert index("BUSY_INLINE_ARG done 2") < index("BUSY_INLINE_ARG caller")
        assert index("BUSY_OBJECT_ARG done 2") < index("BUSY_OBJECT_ARG caller")

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
