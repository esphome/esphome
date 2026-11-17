"""Integration test for wait_until FIFO ordering.

This test verifies that when multiple wait_until actions are queued,
they execute in FIFO (First In First Out) order, not LIFO.

PR #7972 introduced a bug where emplace_front() was used, causing
LIFO ordering which is incorrect.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

COMPLETED_PATTERN = re.compile(r"Completed iteration (\d+)")


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
    waiter = LineWaiter()
    expected_order = [0, 1, 2, 3, 4]

    def completed_order() -> list[int]:
        return [
            int(m.group(1))
            for line in waiter.lines
            if (m := COMPLETED_PATTERN.search(line))
        ][: len(expected_order)]

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "test-wait-until-ordering")

        # Get services
        _, services = await client.list_entities_services()
        test_service = require_service(services, "test_wait_until_fifo")

        # Execute the test
        await client.execute_service(test_service, {})

        # Wait for test to complete
        await wait_or_fail(
            waiter.wait_for_each(*(f"Completed iteration {i}" for i in expected_order)),
            5.0,
            "Test timed out. Expected 5 completions",
            completed_order=completed_order,
        )

        # Verify FIFO order
        order = completed_order()
        assert order == expected_order, (
            f"Unexpected order: {order}. Expected FIFO order: {expected_order}"
        )
