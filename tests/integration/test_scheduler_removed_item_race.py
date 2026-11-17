"""Test for scheduler race condition where removed items still execute."""

import asyncio
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

NORMAL_COUNT = re.compile(r"Normal items executed: (\d+)")
REMOVED_COUNT = re.compile(r"Removed items executed: (\d+)")


def _last_count(pattern: re.Pattern[str], lines: list[str]) -> int:
    counts = [int(m.group(1)) for line in lines if (m := pattern.search(line))]
    return counts[-1] if counts else 0


@pytest.mark.asyncio
async def test_scheduler_removed_item_race(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that items marked for removal don't execute.

    This test verifies the fix for a race condition where:
    1. cleanup_() only removes items from the front of the heap
    2. Items in the middle of the heap marked for removal still execute
    3. This causes cancelled timeouts to run when they shouldn't
    """

    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "scheduler-removed-item-race")

        # List services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find run_test service
        run_test_service = require_service(services, "run_test")

        # Execute the test
        await client.execute_service(run_test_service, {})

        # Wait for test completion
        await wait_or_fail(
            waiter.wait_for("=== Test Complete ==="),
            5.0,
            "Test did not complete within timeout",
        )

        # RACE lines come before the verdict, so the verdict alone decides
        test_passed = any("TEST PASSED" in line for line in waiter.lines)
        normal_executed = _last_count(NORMAL_COUNT, waiter.lines)
        removed_executed = _last_count(REMOVED_COUNT, waiter.lines)
        assert test_passed, (
            f"Test failed! Removed items executed: {removed_executed}, "
            f"Normal items executed: {normal_executed}"
        )
        assert removed_executed == 0, (
            f"Cancelled items should not execute, but {removed_executed} did"
        )
        assert normal_executed == 4, (
            f"Expected 4 normal items to execute, got {normal_executed}"
        )
