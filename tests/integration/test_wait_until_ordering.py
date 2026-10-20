"""Integration test for wait_until FIFO ordering.

This test verifies that when multiple wait_until actions are queued,
they execute in FIFO (First In First Out) order, not LIFO.

PR #7972 introduced a bug where emplace_front() was used, causing
LIFO ordering which is incorrect.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_wait_until_fifo_ordering(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that wait_until executes queued items in FIFO order.

    With the bug (using emplace_front), the order would be 4,3,2,1,0 (LIFO).
    With the fix (using emplace_back), the order should be 0,1,2,3,4 (FIFO).
    """
    test_complete = asyncio.Event()

    # Track completion order
    completed_order = []

    # Patterns to match
    queuing_pattern = re.compile(r"Queueing iteration (\d+)")
    completed_pattern = re.compile(r"Completed iteration (\d+)")

    def check_output(line: str) -> None:
        """Check log output for completion order."""
        if test_complete.is_set():
            return

        if mo := queuing_pattern.search(line):
            iteration = int(mo.group(1))

        elif mo := completed_pattern.search(line):
            iteration = int(mo.group(1))
            completed_order.append(iteration)

            # Test completes when all 5 have completed
            if len(completed_order) == 5:
                test_complete.set()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "test-wait-until-ordering")

        # Get services
        _, services = await client.list_entities_services()
        test_service = next(
            (s for s in services if s.name == "test_wait_until_fifo"), None
        )
        assert test_service is not None, "test_wait_until_fifo service not found"

        # Execute the test
        await client.execute_service(test_service, {})

        # Wait for test to complete
        await wait_or_fail(
            test_complete,
            5.0,
            "Test timed out. Expected 5 completions",
            completed_order=completed_order,
        )

        # Verify FIFO order
        expected_order = [0, 1, 2, 3, 4]
        assert completed_order == expected_order, (
            f"Unexpected order: {completed_order}. "
            f"Expected FIFO order: {expected_order}"
        )
