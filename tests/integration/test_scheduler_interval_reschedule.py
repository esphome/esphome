"""Test that intervals are correctly rescheduled after firing.

This test verifies the optimization where fired intervals are pushed directly
back into the scheduler's heap (items_) via push_back() + push_heap(), instead
of routing through the to_add_ staging vector and process_to_add_slow_path_().

Key scenarios tested:
1. Multiple intervals at different periods all fire at correct rates
2. Heap ordering is preserved — faster intervals fire proportionally more often
3. An interval that cancels itself mid-callback is not rescheduled
4. A timeout scheduled from within an interval callback (to_add_ path) still works
5. An interval that replaces itself via set_interval from within its callback
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_scheduler_interval_reschedule(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that intervals are correctly rescheduled via direct heap insertion."""
    # Futures for each milestone
    waiter = LineWaiter()
    fast_10_future = waiter.expect("FAST_10_REACHED")
    medium_5_future = waiter.expect(re.compile(r"MEDIUM_5_REACHED fast_count=(\d+)"))
    slow_3_future = waiter.expect(
        re.compile(r"SLOW_3_REACHED fast_count=(\d+) medium_count=(\d+)")
    )
    self_cancel_stopped_future = waiter.expect("SELF_CANCEL_STOPPED")
    callback_timeout_future = waiter.expect("CALLBACK_TIMEOUT_FIRED")
    replace_original_future = waiter.expect("REPLACE_ORIGINAL_FIRE")
    replaced_stopped_future = waiter.expect("REPLACED_STOPPED")

    def last_count(pattern: re.Pattern[str]) -> int:
        """Return the count logged by the last line matching ``pattern``, or 0."""
        counts = [
            int(m.group(1)) for line in waiter.lines if (m := pattern.search(line))
        ]
        return counts[-1] if counts else 0

    self_cancel_fire = re.compile(r"SELF_CANCEL_FIRE count=(\d+)")
    replaced_fire = re.compile(r"REPLACED_FIRE count=(\d+)")

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "sched-interval-resched")

        # 1. Fast interval (50ms) should reach 10 fires within ~600ms
        await waiter.wait(
            fast_10_future, 5.0, "Fast interval (50ms) did not fire 10 times"
        )

        # 2. Medium interval (100ms) should reach 5 fires
        #    At that point, fast_count should be roughly 2x medium_count
        result = await waiter.wait(
            medium_5_future, 5.0, "Medium interval (100ms) did not fire 5 times"
        )

        fast_at_medium_5 = int(result.group(1))
        # Fast runs at 50ms, medium at 100ms, so fast should be ~2x medium
        # Allow some slack for scheduling jitter
        assert fast_at_medium_5 >= 7, (
            f"Fast interval should have fired at least 7 times when medium hit 5, "
            f"but only fired {fast_at_medium_5} times"
        )

        # 3. Slow interval (200ms) should reach 3 fires
        #    At that point, both fast and medium should have proportionally more fires
        result = await waiter.wait(
            slow_3_future, 5.0, "Slow interval (200ms) did not fire 3 times"
        )

        fast_at_slow_3, medium_at_slow_3 = map(int, result.groups())
        # At 600ms: fast ~12, medium ~6, slow 3
        assert fast_at_slow_3 >= 8, (
            f"Fast should have fired at least 8 times when slow hit 3, "
            f"but only fired {fast_at_slow_3}"
        )
        assert medium_at_slow_3 >= 4, (
            f"Medium should have fired at least 4 times when slow hit 3, "
            f"but only fired {medium_at_slow_3}"
        )

        # 4. Self-cancelling interval should have stopped after exactly 3 fires
        await waiter.wait(
            self_cancel_stopped_future, 5.0, "Self-cancelling interval did not stop"
        )

        # Wait a bit to ensure it doesn't fire again
        await asyncio.sleep(0.3)
        self_cancel_fire_count = last_count(self_cancel_fire)
        assert self_cancel_fire_count == 3, (
            f"Self-cancelling interval fired {self_cancel_fire_count} times, "
            f"expected exactly 3"
        )

        # 5. Timeout scheduled from interval callback should have fired
        await waiter.wait(
            callback_timeout_future,
            5.0,
            "Timeout scheduled from interval callback did not fire",
        )

        # 6. Interval that replaces itself via set_interval from within callback
        #    The original fires once, sets up a new named interval, then stops itself.
        #    The replacement interval should fire 3 times then cancel itself.
        await waiter.wait(
            replace_original_future, 5.0, "Replace-test original interval did not fire"
        )

        await wait_or_fail(
            replaced_stopped_future,
            5.0,
            "Replaced interval did not stop",
            fired=lambda: last_count(replaced_fire),
        )

        # Wait to ensure replacement doesn't fire again after cancellation
        await asyncio.sleep(0.3)
        replaced_fire_count = last_count(replaced_fire)
        assert replaced_fire_count == 3, (
            f"Replaced interval fired {replaced_fire_count} times, expected exactly 3"
        )
