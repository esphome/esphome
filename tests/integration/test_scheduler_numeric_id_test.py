"""Test scheduler numeric ID (uint32_t) overloads."""

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

INTERVAL_COUNT_PATTERN = re.compile(r"Numeric interval 2001 fired.*count: (\d+)")
FINAL_RESULTS_PATTERN = re.compile(r"Timeouts: (\d+), Intervals: (\d+), Defers: (\d+)")


@pytest.mark.asyncio
async def test_scheduler_numeric_id_test(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler handles numeric IDs (uint32_t) correctly."""
    waiter = LineWaiter()

    async def expect(text: str, timeout: float, msg: str) -> str:
        return await wait_or_fail(waiter.wait_for(text), timeout, msg)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "scheduler-numeric-id-test")

        # Wait for numeric timeout tests
        await expect(
            "Numeric timeout 1001 fired",
            0.5,
            "Numeric timeout 1001 did not fire within 0.5 seconds",
        )
        await expect(
            "Numeric timeout 1002 fired",
            0.5,
            "Numeric timeout 1002 did not fire within 0.5 seconds",
        )
        await expect(
            "Numeric interval 2001 fired",
            1.0,
            "Numeric interval 2001 did not fire within 1 second",
        )
        await expect(
            "Cancelled numeric interval 2001",
            2.0,
            "Numeric interval 2001 was not cancelled within 2 seconds",
        )

        # Verify numeric interval ran at least twice
        interval_counts = [
            int(m.group(1))
            for line in waiter.lines
            if (m := INTERVAL_COUNT_PATTERN.search(line))
        ]
        numeric_interval_count = interval_counts[-1] if interval_counts else 0
        assert numeric_interval_count >= 2, (
            f"Expected numeric interval to run at least 2 times, got {numeric_interval_count}"
        )

        # Verify numeric timeout was cancelled
        assert any("Cancelled numeric timeout 3001" in line for line in waiter.lines), (
            "Numeric timeout 3001 should have been cancelled"
        )

        # Wait for duplicate timeout (only last one should fire)
        await expect(
            "Duplicate numeric timeout 4 fired",
            1.0,
            "Duplicate numeric timeout did not fire within 1 second",
        )

        # Wait for component method tests
        await expect(
            "Component numeric timeout 5001 fired",
            0.5,
            "Component numeric timeout did not fire within 0.5 seconds",
        )
        await expect(
            "Component numeric interval 5002 fired",
            1.0,
            "Component numeric interval did not fire within 1 second",
        )

        # Wait for edge case tests
        await expect(
            "Numeric timeout with ID 0 fired",
            0.5,
            "Zero ID timeout did not fire within 0.5 seconds",
        )
        await expect(
            "Numeric timeout with max ID fired",
            0.5,
            "Max ID timeout did not fire within 0.5 seconds",
        )

        # Wait for numeric defer tests
        await expect(
            "Component numeric defer 7001 fired",
            0.5,
            "Numeric defer 7001 did not fire within 0.5 seconds",
        )
        await expect(
            "Component numeric defer 7002 fired",
            0.5,
            "Numeric defer 7002 did not fire within 0.5 seconds",
        )

        # Verify numeric defer was cancelled
        await expect(
            "Cancelled numeric defer 8001: true",
            0.5,
            "Numeric defer 8001 cancel confirmation not received",
        )

        # Wait for final results
        final_line = await expect(
            "Final results", 3.0, "Final results were not logged within 3 seconds"
        )
        assert (match := FINAL_RESULTS_PATTERN.search(final_line)), final_line
        timeout_count, interval_count, defer_count = map(int, match.groups())

        # Verify results
        assert timeout_count >= 6, f"Expected at least 6 timeouts, got {timeout_count}"
        assert interval_count >= 3, (
            f"Expected at least 3 interval fires, got {interval_count}"
        )
        assert defer_count >= 2, f"Expected at least 2 defer fires, got {defer_count}"
