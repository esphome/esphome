"""Test ESPHome queued script functionality."""

from __future__ import annotations

import asyncio
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_queued(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test comprehensive queued script functionality."""
    waiter = LineWaiter()

    def captured(pattern: re.Pattern[str]) -> list[str]:
        """Return the first group of every line matching ``pattern`` so far."""
        return [m.group(1) for line in waiter.lines if (m := pattern.search(line))]

    def items(pattern: re.Pattern[str]) -> list[int]:
        """Return the distinct item numbers ``pattern`` captured, in first-seen order."""
        return list(dict.fromkeys(int(item) for item in captured(pattern)))

    # Patterns for Test 1: Queue depth
    queue_start = re.compile(r"Queue test: START item (\d+)")
    queue_reject = "Script 'queue_depth_script' max instances"

    # Patterns for Test 2: Ring buffer
    ring_start = re.compile(r"Ring buffer: START '([A-Z])'")
    ring_end = re.compile(r"Ring buffer: END '([A-Z])'")

    # Patterns for Test 3: Stop
    stop_start = re.compile(r"Stop test: START (\d+)")

    # Patterns for Test 4: Rejection
    reject_start = re.compile(r"Rejection test: START (\d+)")
    reject_reject = "Script 'rejection_script' max instances"

    # Patterns for Test 5: No params
    no_params_end = "No params: END"

    # Patterns for boot script (executed twice from on_boot before setup)
    boot_end = re.compile(r"Boot queued: END (\d+)")

    # Test completion futures
    test1_complete = waiter.expect(re.compile(r"Queue test: END item 5\b"))
    stop_logged = waiter.expect("STOPPING script now")
    test4_complete = waiter.expect(re.compile(r"Rejection test: END 3\b"))
    # Test 6: Re-execute after stop
    test6_complete = waiter.expect(re.compile(r"Stop test: END 9\b"))

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Get services
        _, services = await client.list_entities_services()

        # Boot: both executions from on_boot must complete, including the one
        # that was queued before QueueingScript::setup() ran
        await waiter.wait_count(boot_end, 2, 2.0, "Boot: on_boot runs did not end")
        boot_ended = sorted(int(item) for item in captured(boot_end))
        assert boot_ended == [1, 2], (
            f"Boot: Expected both on_boot executions to complete, got {boot_ended}"
        )

        # Test 1: Queue depth limit
        test_service = require_service(services, "test_queue_depth")
        await client.execute_service(test_service, {})
        await waiter.wait(test1_complete, 2.0, "Test 1: item 5 did not end")
        await asyncio.sleep(0.1)  # Give time for rejections

        # Verify Test 1
        queue_processed = sorted(items(queue_start))
        assert queue_processed == [1, 2, 3, 4, 5], (
            f"Test 1: Expected to process items 1-5 (max_runs=5 means 5 total), got {queue_processed}"
        )
        assert waiter.count(queue_reject) >= 2, (
            "Test 1: Expected at least 2 rejection warnings (items 6-7 should be rejected)"
        )

        # Test 2: Ring buffer order
        test_service = require_service(services, "test_ring_buffer")
        await client.execute_service(test_service, {})
        await waiter.wait_count(ring_end, 3, 2.0, "Test 2: ring buffer did not end")

        # Verify Test 2
        assert captured(ring_start) == ["A", "B", "C"], (
            f"Test 2: Expected start order [A, B, C], got {captured(ring_start)}"
        )
        assert captured(ring_end) == ["A", "B", "C"], (
            f"Test 2: Expected end order [A, B, C], got {captured(ring_end)}"
        )

        # Test 3: Stop clears queue
        test_service = require_service(services, "test_stop_clears")
        await client.execute_service(test_service, {})
        await waiter.wait(stop_logged, 1.7, "Test 3: Stop command was not logged")
        # Give time for any queued items to be cleared
        await asyncio.sleep(0.3)

        # Verify Test 3
        assert items(stop_start) == [1], (
            f"Test 3: Expected only item 1 to process, got {items(stop_start)}"
        )

        # Test 4: Rejection enforcement (max_runs=3)
        test_service = require_service(services, "test_rejection")
        await client.execute_service(test_service, {})
        await waiter.wait(test4_complete, 2.0, "Test 4: item 3 did not end")
        await asyncio.sleep(0.1)  # Give time for rejections

        # Verify Test 4
        reject_processed = sorted(items(reject_start))
        assert reject_processed == [1, 2, 3], (
            f"Test 4: Expected to process items 1-3 (max_runs=3 means 3 total), got {reject_processed}"
        )
        rejections = waiter.count(reject_reject)
        assert rejections == 5, (
            f"Test 4: Expected 5 rejections (items 4-8), got {rejections}"
        )

        # Test 5: No parameters
        test_service = require_service(services, "test_no_params")
        await client.execute_service(test_service, {})
        await waiter.wait_count(no_params_end, 3, 2.0, "Test 5: runs did not end")

        # Verify Test 5
        executions = waiter.count(no_params_end)
        assert executions == 3, f"Test 5: Expected 3 executions, got {executions}"

        # Test 5 again: after the queue fully drained (loop disabled while
        # idle), executing again must still work
        test_service = require_service(services, "test_no_params")
        await client.execute_service(test_service, {})
        await waiter.wait_count(no_params_end, 6, 2.0, "Test 5 again: runs did not end")
        executions = waiter.count(no_params_end)
        assert executions == 6, (
            f"Test 5 again: Expected 6 executions total, got {executions}"
        )

        # Test 6: a stopped script (queue cleared, loop disabled) must run
        # again on the next execute; the future resolves only on "END 9"
        test_service = require_service(services, "test_after_stop")
        await client.execute_service(test_service, {})
        await waiter.wait(test6_complete, 2.0, "Test 6: item 9 did not end")
