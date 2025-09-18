"""Test scheduler string optimization with static and dynamic strings."""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


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
        elif "Cancelled timeout using different string object" in clean_line:
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
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-string-test"

        # Wait for static string tests
        try:
            await asyncio.wait_for(static_timeout_1_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Static timeout 1 did not fire within 0.5 seconds")

        try:
            await asyncio.wait_for(static_timeout_2_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Static timeout 2 did not fire within 0.5 seconds")

        try:
            await asyncio.wait_for(static_interval_fired.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Static interval did not fire within 1 second")

        try:
            await asyncio.wait_for(static_interval_cancelled.wait(), timeout=2.0)
        except TimeoutError:
            pytest.fail("Static interval was not cancelled within 2 seconds")

        # Verify static interval ran at least 3 times
        assert static_interval_count >= 2, (
            f"Expected static interval to run at least 3 times, got {static_interval_count + 1}"
        )

        # Verify static timeout was cancelled
        assert static_timeout_cancelled.is_set(), (
            "Static timeout should have been cancelled"
        )

        # Wait for static defer tests
        try:
            await asyncio.wait_for(static_defer_1_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Static defer 1 did not fire within 0.5 seconds")

        try:
            await asyncio.wait_for(static_defer_2_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Static defer 2 did not fire within 0.5 seconds")

        # Wait for dynamic string tests
        try:
            await asyncio.wait_for(dynamic_timeout_fired.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Dynamic timeout did not fire within 1 second")

        try:
            await asyncio.wait_for(dynamic_interval_fired.wait(), timeout=1.5)
        except TimeoutError:
            pytest.fail("Dynamic interval did not fire within 1.5 seconds")

        # Wait for dynamic defer test
        try:
            await asyncio.wait_for(dynamic_defer_fired.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Dynamic defer did not fire within 1 second")

        # Wait for cancel test
        try:
            await asyncio.wait_for(cancel_test_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Cancel test did not complete within 1 second")

        # Wait for final results
        try:
            await asyncio.wait_for(final_results_logged.wait(), timeout=4.0)
        except TimeoutError:
            pytest.fail("Final results were not logged within 4 seconds")

        # Verify results
        assert timeout_count >= 6, (
            f"Expected at least 6 timeouts (including defers), got {timeout_count}"
        )
        assert interval_count >= 3, (
            f"Expected at least 3 interval fires, got {interval_count}"
        )

        # Empty string timeout DOES fire (scheduler accepts empty names)
        assert empty_string_timeout_fired.is_set(), "Empty string timeout should fire"
