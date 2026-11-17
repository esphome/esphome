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

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_interval_zero_coerced(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """interval=0ms must be coerced to 1ms and not starve the main loop."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # The API-client connection itself is evidence that the main loop
        # is not starved — if set_interval(0) were spinning we could not
        # get here at all.
        await assert_device_name(client, "sched-interval-zero")

        # Coerce warning must fire at registration
        await waiter.wait_for("would spin main loop", timeout=5.0)

        # The coerced 1ms interval should fire 50 times quickly — this
        # confirms the callback actually runs (not just registered) and the
        # scheduler yields back to the main loop each time.
        # A timeout means the coercion failed or the main loop is still starved.
        await waiter.wait_for("ZERO_INTERVAL_50_FIRES_REACHED", timeout=5.0)
