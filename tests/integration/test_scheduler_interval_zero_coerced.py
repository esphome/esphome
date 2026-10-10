"""Test that Scheduler::set_timer_common_ coerces interval=0 to 1ms.

Regression test for the scheduler busy-loop when interval=0 was passed
literally. Without the coercion, Scheduler::call() would spin forever
because the item's next_execution == now_64 after re-scheduling, failing
the loop's `> now_64` break condition. The device would fail to yield
back to the main loop and trigger a WDT reset.

With the coercion, interval=0 becomes interval=1 and the scheduler
fires at ~1kHz (bounded by the loop), the main loop continues to run,
and the device stays responsive to API calls.
"""

from __future__ import annotations

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_interval_zero_coerced(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """interval=0ms must be coerced to 1ms and not starve the main loop."""
    loop = asyncio.get_running_loop()
    reached_50: asyncio.Future[None] = loop.create_future()
    coerce_warning: asyncio.Future[None] = loop.create_future()

    def on_log_line(line: str) -> None:
        if "ZERO_INTERVAL_50_FIRES_REACHED" in line and not reached_50.done():
            reached_50.set_result(None)
        if "would spin main loop" in line and not coerce_warning.done():
            coerce_warning.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # The API-client connection itself is evidence that the main loop
        # is not starved — if set_interval(0) were spinning we could not
        # get here at all.
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "sched-interval-zero"

        # Coerce warning must fire at registration
        try:
            await asyncio.wait_for(coerce_warning, timeout=5.0)
        except TimeoutError:
            pytest.fail("Expected coerce warning 'would spin main loop' not seen")

        # The coerced 1ms interval should fire 50 times quickly — this
        # confirms the callback actually runs (not just registered) and the
        # scheduler yields back to the main loop each time.
        try:
            await asyncio.wait_for(reached_50, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                "Coerced interval=0→1ms did not reach 50 fires within 5s, "
                "which would indicate either the coercion failed or the "
                "main loop is still being starved."
            )
