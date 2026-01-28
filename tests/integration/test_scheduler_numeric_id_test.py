"""Test scheduler numeric ID (uint32_t) overloads."""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_numeric_id_test(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler handles numeric IDs (uint32_t) correctly."""
    # Track counts
    timeout_count = 0
    interval_count = 0
    retry_count = 0

    # Events for each test completion
    numeric_timeout_1001_fired = asyncio.Event()
    numeric_timeout_1002_fired = asyncio.Event()
    numeric_interval_2001_fired = asyncio.Event()
    numeric_interval_cancelled = asyncio.Event()
    numeric_timeout_cancelled = asyncio.Event()
    duplicate_timeout_fired = asyncio.Event()
    component_timeout_fired = asyncio.Event()
    component_interval_fired = asyncio.Event()
    zero_id_timeout_fired = asyncio.Event()
    max_id_timeout_fired = asyncio.Event()
    numeric_retry_done = asyncio.Event()
    numeric_retry_cancelled = asyncio.Event()
    final_results_logged = asyncio.Event()

    # Track interval counts
    numeric_interval_count = 0
    numeric_retry_count = 0

    def on_log_line(line: str) -> None:
        nonlocal timeout_count, interval_count, retry_count
        nonlocal numeric_interval_count, numeric_retry_count

        # Strip ANSI color codes
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        # Check for numeric timeout completions
        if "Numeric timeout 1001 fired" in clean_line:
            numeric_timeout_1001_fired.set()
            timeout_count += 1

        elif "Numeric timeout 1002 fired" in clean_line:
            numeric_timeout_1002_fired.set()
            timeout_count += 1

        # Check for numeric interval
        elif "Numeric interval 2001 fired" in clean_line:
            match = re.search(r"count: (\d+)", clean_line)
            if match:
                numeric_interval_count = int(match.group(1))
                numeric_interval_2001_fired.set()

        elif "Cancelled numeric interval 2001" in clean_line:
            numeric_interval_cancelled.set()

        elif "Cancelled numeric timeout 3001" in clean_line:
            numeric_timeout_cancelled.set()

        # Check for duplicate timeout (only last should fire)
        elif "Duplicate numeric timeout" in clean_line:
            match = re.search(r"timeout (\d+) fired", clean_line)
            if match and match.group(1) == "4":
                duplicate_timeout_fired.set()
                timeout_count += 1

        # Check for component method tests
        elif "Component numeric timeout 5001 fired" in clean_line:
            component_timeout_fired.set()
            timeout_count += 1

        elif "Component numeric interval 5002 fired" in clean_line:
            component_interval_fired.set()
            interval_count += 1

        # Check for edge case tests
        elif "Numeric timeout with ID 0 fired" in clean_line:
            zero_id_timeout_fired.set()
            timeout_count += 1

        elif "Numeric timeout with max ID fired" in clean_line:
            max_id_timeout_fired.set()
            timeout_count += 1

        # Check for numeric retry tests
        elif "Numeric retry 6001 attempt" in clean_line:
            match = re.search(r"attempt (\d+)", clean_line)
            if match:
                numeric_retry_count = int(match.group(1))

        elif "Numeric retry 6001 done" in clean_line:
            numeric_retry_done.set()

        elif "Cancelled numeric retry 6002" in clean_line:
            numeric_retry_cancelled.set()

        # Check for final results
        elif "Final results" in clean_line:
            match = re.search(
                r"Timeouts: (\d+), Intervals: (\d+), Retries: (\d+)", clean_line
            )
            if match:
                timeout_count = int(match.group(1))
                interval_count = int(match.group(2))
                retry_count = int(match.group(3))
                final_results_logged.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-numeric-id-test"

        # Wait for numeric timeout tests
        try:
            await asyncio.wait_for(numeric_timeout_1001_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Numeric timeout 1001 did not fire within 0.5 seconds")

        try:
            await asyncio.wait_for(numeric_timeout_1002_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Numeric timeout 1002 did not fire within 0.5 seconds")

        try:
            await asyncio.wait_for(numeric_interval_2001_fired.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Numeric interval 2001 did not fire within 1 second")

        try:
            await asyncio.wait_for(numeric_interval_cancelled.wait(), timeout=2.0)
        except TimeoutError:
            pytest.fail("Numeric interval 2001 was not cancelled within 2 seconds")

        # Verify numeric interval ran at least twice
        assert numeric_interval_count >= 2, (
            f"Expected numeric interval to run at least 2 times, got {numeric_interval_count}"
        )

        # Verify numeric timeout was cancelled
        assert numeric_timeout_cancelled.is_set(), (
            "Numeric timeout 3001 should have been cancelled"
        )

        # Wait for duplicate timeout (only last one should fire)
        try:
            await asyncio.wait_for(duplicate_timeout_fired.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Duplicate numeric timeout did not fire within 1 second")

        # Wait for component method tests
        try:
            await asyncio.wait_for(component_timeout_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Component numeric timeout did not fire within 0.5 seconds")

        try:
            await asyncio.wait_for(component_interval_fired.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Component numeric interval did not fire within 1 second")

        # Wait for edge case tests
        try:
            await asyncio.wait_for(zero_id_timeout_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Zero ID timeout did not fire within 0.5 seconds")

        try:
            await asyncio.wait_for(max_id_timeout_fired.wait(), timeout=0.5)
        except TimeoutError:
            pytest.fail("Max ID timeout did not fire within 0.5 seconds")

        # Wait for numeric retry tests
        try:
            await asyncio.wait_for(numeric_retry_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail(
                f"Numeric retry 6001 did not complete. Count: {numeric_retry_count}"
            )

        assert numeric_retry_count >= 2, (
            f"Expected at least 2 numeric retry attempts, got {numeric_retry_count}"
        )

        # Verify numeric retry was cancelled
        assert numeric_retry_cancelled.is_set(), (
            "Numeric retry 6002 should have been cancelled"
        )

        # Wait for final results
        try:
            await asyncio.wait_for(final_results_logged.wait(), timeout=3.0)
        except TimeoutError:
            pytest.fail("Final results were not logged within 3 seconds")

        # Verify results
        assert timeout_count >= 6, f"Expected at least 6 timeouts, got {timeout_count}"
        assert interval_count >= 3, (
            f"Expected at least 3 interval fires, got {interval_count}"
        )
        assert retry_count >= 2, (
            f"Expected at least 2 retry attempts, got {retry_count}"
        )
