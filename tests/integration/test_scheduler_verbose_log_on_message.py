"""Test that a logger on_message automation can use the scheduler from a scheduler log.

Regression test for a self-deadlock: Scheduler::set_timer_common_ logged a new
interval at VERBOSE while holding the scheduler lock. The logger calls on_message
listeners on the same thread, and an automation with a delay adds a scheduler
timeout, which took the same non-recursive lock again. The first set_interval()
in setup() hung the main loop.
"""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_verbose_log_on_message(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The main loop keeps running when on_message reacts to a scheduler log."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "sched-verbose-log")
        await waiter.wait_for("Scheduler interval for", timeout=5.0)
        await waiter.wait_for("INTERVAL_5_FIRES_REACHED", timeout=5.0)
