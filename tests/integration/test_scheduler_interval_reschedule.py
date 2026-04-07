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

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_interval_reschedule(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that intervals are correctly rescheduled via direct heap insertion."""
    loop = asyncio.get_running_loop()

    # Futures for each milestone
    fast_10_future: asyncio.Future[None] = loop.create_future()
    medium_5_future: asyncio.Future[tuple[int]] = loop.create_future()
    slow_3_future: asyncio.Future[tuple[int, int]] = loop.create_future()
    self_cancel_stopped_future: asyncio.Future[None] = loop.create_future()
    callback_timeout_future: asyncio.Future[None] = loop.create_future()
    replace_original_future: asyncio.Future[None] = loop.create_future()
    replaced_stopped_future: asyncio.Future[None] = loop.create_future()

    self_cancel_fire_count = 0
    replaced_fire_count = 0

    def on_log_line(line: str) -> None:
        nonlocal self_cancel_fire_count, replaced_fire_count

        if "FAST_10_REACHED" in line and not fast_10_future.done():
            fast_10_future.set_result(None)

        match = re.search(r"MEDIUM_5_REACHED fast_count=(\d+)", line)
        if match and not medium_5_future.done():
            medium_5_future.set_result((int(match.group(1)),))

        match = re.search(r"SLOW_3_REACHED fast_count=(\d+) medium_count=(\d+)", line)
        if match and not slow_3_future.done():
            slow_3_future.set_result((int(match.group(1)), int(match.group(2))))

        match = re.search(r"SELF_CANCEL_FIRE count=(\d+)", line)
        if match:
            self_cancel_fire_count = int(match.group(1))

        if "SELF_CANCEL_STOPPED" in line and not self_cancel_stopped_future.done():
            self_cancel_stopped_future.set_result(None)

        if "CALLBACK_TIMEOUT_FIRED" in line and not callback_timeout_future.done():
            callback_timeout_future.set_result(None)

        if "REPLACE_ORIGINAL_FIRE" in line and not replace_original_future.done():
            replace_original_future.set_result(None)

        match = re.search(r"REPLACED_FIRE count=(\d+)", line)
        if match:
            replaced_fire_count = int(match.group(1))

        if "REPLACED_STOPPED" in line and not replaced_stopped_future.done():
            replaced_stopped_future.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "sched-interval-resched"

        # 1. Fast interval (50ms) should reach 10 fires within ~600ms
        try:
            await asyncio.wait_for(fast_10_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Fast interval (50ms) did not fire 10 times")

        # 2. Medium interval (100ms) should reach 5 fires
        #    At that point, fast_count should be roughly 2x medium_count
        try:
            result = await asyncio.wait_for(medium_5_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Medium interval (100ms) did not fire 5 times")

        fast_at_medium_5 = result[0]
        # Fast runs at 50ms, medium at 100ms, so fast should be ~2x medium
        # Allow some slack for scheduling jitter
        assert fast_at_medium_5 >= 7, (
            f"Fast interval should have fired at least 7 times when medium hit 5, "
            f"but only fired {fast_at_medium_5} times"
        )

        # 3. Slow interval (200ms) should reach 3 fires
        #    At that point, both fast and medium should have proportionally more fires
        try:
            result = await asyncio.wait_for(slow_3_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Slow interval (200ms) did not fire 3 times")

        fast_at_slow_3, medium_at_slow_3 = result
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
        try:
            await asyncio.wait_for(self_cancel_stopped_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Self-cancelling interval did not stop")

        # Wait a bit to ensure it doesn't fire again
        await asyncio.sleep(0.3)
        assert self_cancel_fire_count == 3, (
            f"Self-cancelling interval fired {self_cancel_fire_count} times, "
            f"expected exactly 3"
        )

        # 5. Timeout scheduled from interval callback should have fired
        try:
            await asyncio.wait_for(callback_timeout_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Timeout scheduled from interval callback did not fire")

        # 6. Interval that replaces itself via set_interval from within callback
        #    The original fires once, sets up a new named interval, then stops itself.
        #    The replacement interval should fire 3 times then cancel itself.
        try:
            await asyncio.wait_for(replace_original_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Replace-test original interval did not fire")

        try:
            await asyncio.wait_for(replaced_stopped_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Replaced interval did not stop. Fired {replaced_fire_count} times"
            )

        # Wait to ensure replacement doesn't fire again after cancellation
        await asyncio.sleep(0.3)
        assert replaced_fire_count == 3, (
            f"Replaced interval fired {replaced_fire_count} times, expected exactly 3"
        )
