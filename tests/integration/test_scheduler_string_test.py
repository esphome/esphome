"""Test scheduler string optimization with static and dynamic strings."""

import asyncio
import re

import pytest

from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_scheduler_string_test(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler handles both static and dynamic strings correctly."""
    # Track counts
    timeout_count = 0
    interval_count = 0

    # Events for each test completion
    static_timeout_1_fired = asyncio.Event()
    static_timeout_2_fired = asyncio.Event()
    static_interval_fired = asyncio.Event()
    static_interval_cancelled = asyncio.Event()
    empty_string_timeout_fired = asyncio.Event()
    static_timeout_cancelled = asyncio.Event()
    static_defer_1_fired = asyncio.Event()
    static_defer_2_fired = asyncio.Event()
    dynamic_timeout_fired = asyncio.Event()
    dynamic_interval_fired = asyncio.Event()
    dynamic_defer_fired = asyncio.Event()
    cancel_test_done = asyncio.Event()
    final_results_logged = asyncio.Event()

    # Track interval counts
    static_interval_count = 0
    dynamic_interval_count = 0

    def on_log_line(line: str) -> None:
        nonlocal \
            timeout_count, \
            interval_count, \
            static_interval_count, \
            dynamic_interval_count

        # Strip ANSI color codes
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        # Check for static timeout completions
        if "Static timeout 1 fired" in clean_line:
            static_timeout_1_fired.set()
            timeout_count += 1

        elif "Static timeout 2 fired" in clean_line:
            static_timeout_2_fired.set()
            timeout_count += 1

        # Check for static interval
        elif "Static interval 1 fired" in clean_line:
            match = re.search(r"count: (\d+)", clean_line)
            if match:
                static_interval_count = int(match.group(1))
                static_interval_fired.set()

        elif "Cancelled static interval 1" in clean_line:
            static_interval_cancelled.set()

        # Check for empty string timeout
        elif "Empty string timeout fired" in clean_line:
            empty_string_timeout_fired.set()

        # Check for static timeout cancellation
        elif "Cancelled static timeout using const char*" in clean_line:
            static_timeout_cancelled.set()

        # Check for static defer tests
        elif "Static defer 1 fired" in clean_line:
            static_defer_1_fired.set()
            timeout_count += 1

        elif "Static defer 2 fired" in clean_line:
            static_defer_2_fired.set()
            timeout_count += 1

        # Check for dynamic string tests
        elif "Dynamic timeout fired" in clean_line:
            dynamic_timeout_fired.set()
            timeout_count += 1

        elif "Dynamic interval fired" in clean_line:
            dynamic_interval_count += 1
            dynamic_interval_fired.set()

        # Check for dynamic defer test
        elif "Dynamic defer fired" in clean_line:
            dynamic_defer_fired.set()
            timeout_count += 1

        # Check for cancel test
        elif "Cancelled timeout using different buffer with same content" in clean_line:
            cancel_test_done.set()

        # Check for final results
        elif "Final results" in clean_line:
            match = re.search(r"Timeouts: (\d+), Intervals: (\d+)", clean_line)
            if match:
                timeout_count = int(match.group(1))
                interval_count = int(match.group(2))
                final_results_logged.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "scheduler-string-test")

        # Wait for static string tests
        await wait_or_fail(
            static_timeout_1_fired,
            0.5,
            "Static timeout 1 did not fire within 0.5 seconds",
        )

        await wait_or_fail(
            static_timeout_2_fired,
            0.5,
            "Static timeout 2 did not fire within 0.5 seconds",
        )

        await wait_or_fail(
            static_interval_fired, 1.0, "Static interval did not fire within 1 second"
        )

        await wait_or_fail(
            static_interval_cancelled,
            2.0,
            "Static interval was not cancelled within 2 seconds",
        )

        # Verify static interval ran at least 3 times
        assert static_interval_count >= 2, (
            f"Expected static interval to run at least 3 times, got {static_interval_count + 1}"
        )

        # Verify static timeout was cancelled
        assert static_timeout_cancelled.is_set(), (
            "Static timeout should have been cancelled"
        )

        # Wait for static defer tests
        await wait_or_fail(
            static_defer_1_fired, 0.5, "Static defer 1 did not fire within 0.5 seconds"
        )

        await wait_or_fail(
            static_defer_2_fired, 0.5, "Static defer 2 did not fire within 0.5 seconds"
        )

        # Wait for dynamic string tests
        await wait_or_fail(
            dynamic_timeout_fired, 1.0, "Dynamic timeout did not fire within 1 second"
        )

        await wait_or_fail(
            dynamic_interval_fired,
            1.5,
            "Dynamic interval did not fire within 1.5 seconds",
        )

        # Wait for dynamic defer test
        await wait_or_fail(
            dynamic_defer_fired, 1.0, "Dynamic defer did not fire within 1 second"
        )

        # Wait for cancel test
        await wait_or_fail(
            cancel_test_done, 1.0, "Cancel test did not complete within 1 second"
        )

        # Wait for final results
        await wait_or_fail(
            final_results_logged, 4.0, "Final results were not logged within 4 seconds"
        )

        # Verify results
        assert timeout_count >= 6, (
            f"Expected at least 6 timeouts (including defers), got {timeout_count}"
        )
        assert interval_count >= 3, (
            f"Expected at least 3 interval fires, got {interval_count}"
        )

        # Empty string timeout DOES fire (scheduler accepts empty names)
        assert empty_string_timeout_fired.is_set(), "Empty string timeout should fire"
