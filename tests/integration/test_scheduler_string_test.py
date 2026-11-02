"""Test scheduler string optimization with static and dynamic strings."""

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

INTERVAL_COUNT_PATTERN = re.compile(r"Static interval 1 fired.*count: (\d+)")
FINAL_RESULTS_PATTERN = re.compile(r"Timeouts: (\d+), Intervals: (\d+)")


@pytest.mark.asyncio
async def test_scheduler_string_test(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler handles both static and dynamic strings correctly."""
    waiter = LineWaiter()

    async def expect(text: str, timeout: float, msg: str) -> str:
        return await wait_or_fail(waiter.wait_for(text), timeout, msg)

    def seen(text: str) -> bool:
        return any(text in line for line in waiter.lines)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "scheduler-string-test")

        # Wait for static string tests
        await expect(
            "Static timeout 1 fired",
            0.5,
            "Static timeout 1 did not fire within 0.5 seconds",
        )
        await expect(
            "Static timeout 2 fired",
            0.5,
            "Static timeout 2 did not fire within 0.5 seconds",
        )
        await expect(
            "Static interval 1 fired",
            1.0,
            "Static interval did not fire within 1 second",
        )
        await expect(
            "Cancelled static interval 1",
            2.0,
            "Static interval was not cancelled within 2 seconds",
        )

        # Verify static interval ran at least 3 times
        interval_counts = [
            int(m.group(1))
            for line in waiter.lines
            if (m := INTERVAL_COUNT_PATTERN.search(line))
        ]
        static_interval_count = interval_counts[-1] if interval_counts else 0
        assert static_interval_count >= 2, (
            f"Expected static interval to run at least 3 times, got {static_interval_count + 1}"
        )

        # Verify static timeout was cancelled
        assert seen("Cancelled static timeout using const char*"), (
            "Static timeout should have been cancelled"
        )

        # Wait for static defer tests
        await expect(
            "Static defer 1 fired",
            0.5,
            "Static defer 1 did not fire within 0.5 seconds",
        )
        await expect(
            "Static defer 2 fired",
            0.5,
            "Static defer 2 did not fire within 0.5 seconds",
        )

        # Wait for dynamic string tests
        await expect(
            "Dynamic timeout fired", 1.0, "Dynamic timeout did not fire within 1 second"
        )
        await expect(
            "Dynamic interval fired",
            1.5,
            "Dynamic interval did not fire within 1.5 seconds",
        )

        # Wait for dynamic defer test
        await expect(
            "Dynamic defer fired", 1.0, "Dynamic defer did not fire within 1 second"
        )

        # Wait for cancel test
        await expect(
            "Cancelled timeout using different buffer with same content",
            1.0,
            "Cancel test did not complete within 1 second",
        )

        # Wait for final results
        final_line = await expect(
            "Final results", 4.0, "Final results were not logged within 4 seconds"
        )
        assert (match := FINAL_RESULTS_PATTERN.search(final_line)), final_line
        timeout_count, interval_count = map(int, match.groups())

        # Verify results
        assert timeout_count >= 6, (
            f"Expected at least 6 timeouts (including defers), got {timeout_count}"
        )
        assert interval_count >= 3, (
            f"Expected at least 3 interval fires, got {interval_count}"
        )

        # Empty string timeout DOES fire (scheduler accepts empty names)
        assert seen("Empty string timeout fired"), "Empty string timeout should fire"
