"""Test scheduler retry functionality."""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_retry_test(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler retry functionality works correctly."""
    # Track test progress
    simple_retry_done = asyncio.Event()
    backoff_retry_done = asyncio.Event()
    immediate_done_done = asyncio.Event()
    cancel_retry_done = asyncio.Event()
    empty_name_retry_done = asyncio.Event()
    component_retry_done = asyncio.Event()
    multiple_name_done = asyncio.Event()
    const_char_done = asyncio.Event()
    static_char_done = asyncio.Event()
    mixed_cancel_done = asyncio.Event()
    test_complete = asyncio.Event()

    # Track retry counts
    simple_retry_count = 0
    backoff_retry_count = 0
    immediate_done_count = 0
    cancel_retry_count = 0
    empty_name_retry_count = 0
    component_retry_count = 0
    multiple_name_count = 0
    const_char_retry_count = 0
    static_char_retry_count = 0

    # Track specific test results
    cancel_result = None
    empty_cancel_result = None
    mixed_cancel_result = None
    backoff_intervals = []

    def on_log_line(line: str) -> None:
        nonlocal simple_retry_count, backoff_retry_count, immediate_done_count
        nonlocal cancel_retry_count, empty_name_retry_count, component_retry_count
        nonlocal multiple_name_count, const_char_retry_count, static_char_retry_count
        nonlocal cancel_result, empty_cancel_result, mixed_cancel_result

        # Strip ANSI color codes
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        # Simple retry test
        if "Simple retry attempt" in clean_line:
            if match := re.search(r"Simple retry attempt (\d+)", clean_line):
                simple_retry_count = int(match.group(1))

        elif "Simple retry succeeded on attempt" in clean_line:
            simple_retry_done.set()

        # Backoff retry test
        elif "Backoff retry attempt" in clean_line:
            if match := re.search(
                r"Backoff retry attempt (\d+).*interval=(\d+)ms", clean_line
            ):
                backoff_retry_count = int(match.group(1))
                interval = int(match.group(2))
                if backoff_retry_count > 1:  # Skip first (immediate) call
                    backoff_intervals.append(interval)

        elif "Backoff retry completed" in clean_line:
            backoff_retry_done.set()

        # Immediate done test
        elif "Immediate done retry called" in clean_line:
            immediate_done_count += 1
            immediate_done_done.set()

        # Cancel retry test
        elif "Cancel test retry attempt" in clean_line:
            cancel_retry_count += 1

        elif "Retry cancellation result:" in clean_line:
            cancel_result = "true" in clean_line
            cancel_retry_done.set()

        # Empty name retry test
        elif "Empty name retry attempt" in clean_line:
            if match := re.search(r"Empty name retry attempt (\d+)", clean_line):
                empty_name_retry_count = int(match.group(1))

        elif "Empty name retry cancel result:" in clean_line:
            empty_cancel_result = "true" in clean_line

        elif "Empty name retry ran" in clean_line:
            empty_name_retry_done.set()

        # Component retry test
        elif "Component retry attempt" in clean_line:
            if match := re.search(r"Component retry attempt (\d+)", clean_line):
                component_retry_count = int(match.group(1))
                if component_retry_count >= 2:
                    component_retry_done.set()

        # Multiple same name test
        elif "Second duplicate retry attempt" in clean_line:
            if match := re.search(r"counter=(\d+)", clean_line):
                multiple_name_count = int(match.group(1))
                if multiple_name_count >= 20:
                    multiple_name_done.set()

        # Const char retry test
        elif "Const char retry" in clean_line:
            if match := re.search(r"Const char retry (\d+)", clean_line):
                const_char_retry_count = int(match.group(1))
                const_char_done.set()

        # Static const char retry test
        elif "Static const char retry" in clean_line:
            if match := re.search(r"Static const char retry (\d+)", clean_line):
                static_char_retry_count = int(match.group(1))
                static_char_done.set()

        elif "Static cancel result:" in clean_line:
            # This is part of test 9, but we don't track it separately
            pass

        # Mixed cancel test
        elif "Mixed cancel result:" in clean_line:
            mixed_cancel_result = "true" in clean_line
            mixed_cancel_done.set()

        # Test completion
        elif "All retry tests completed" in clean_line:
            test_complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-retry-test"

        # Wait for simple retry test
        try:
            await asyncio.wait_for(simple_retry_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail(
                f"Simple retry test did not complete. Count: {simple_retry_count}"
            )

        assert simple_retry_count == 2, (
            f"Expected 2 simple retry attempts, got {simple_retry_count}"
        )

        # Wait for backoff retry test
        try:
            await asyncio.wait_for(backoff_retry_done.wait(), timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Backoff retry test did not complete. Count: {backoff_retry_count}"
            )

        assert backoff_retry_count == 4, (
            f"Expected 4 backoff retry attempts, got {backoff_retry_count}"
        )

        # Verify backoff intervals (allowing for timing variations)
        assert len(backoff_intervals) >= 2, (
            f"Expected at least 2 intervals, got {len(backoff_intervals)}"
        )
        if len(backoff_intervals) >= 3:
            # First interval should be ~50ms (very wide tolerance for heavy system load)
            assert 20 <= backoff_intervals[0] <= 150, (
                f"First interval {backoff_intervals[0]}ms not ~50ms"
            )
            # Second interval should be ~100ms (50ms * 2.0)
            assert 50 <= backoff_intervals[1] <= 250, (
                f"Second interval {backoff_intervals[1]}ms not ~100ms"
            )
            # Third interval should be ~200ms (100ms * 2.0)
            assert 100 <= backoff_intervals[2] <= 500, (
                f"Third interval {backoff_intervals[2]}ms not ~200ms"
            )

        # Wait for immediate done test
        try:
            await asyncio.wait_for(immediate_done_done.wait(), timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Immediate done test did not complete. Count: {immediate_done_count}"
            )

        assert immediate_done_count == 1, (
            f"Expected 1 immediate done call, got {immediate_done_count}"
        )

        # Wait for cancel retry test
        try:
            await asyncio.wait_for(cancel_retry_done.wait(), timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Cancel retry test did not complete. Count: {cancel_retry_count}"
            )

        assert cancel_result is True, "Retry cancellation should have succeeded"
        assert 2 <= cancel_retry_count <= 5, (
            f"Expected 2-5 cancel retry attempts before cancellation, got {cancel_retry_count}"
        )

        # Wait for empty name retry test
        try:
            await asyncio.wait_for(empty_name_retry_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail(
                f"Empty name retry test did not complete. Count: {empty_name_retry_count}"
            )

        # Empty name retry should run at least once before being cancelled
        assert 1 <= empty_name_retry_count <= 3, (
            f"Expected 1-3 empty name retry attempts, got {empty_name_retry_count}"
        )
        assert empty_cancel_result is True, (
            "Empty name retry cancel should have succeeded"
        )

        # Wait for component retry test
        try:
            await asyncio.wait_for(component_retry_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail(
                f"Component retry test did not complete. Count: {component_retry_count}"
            )

        assert component_retry_count >= 2, (
            f"Expected at least 2 component retry attempts, got {component_retry_count}"
        )

        # Wait for multiple same name test
        try:
            await asyncio.wait_for(multiple_name_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail(
                f"Multiple same name test did not complete. Count: {multiple_name_count}"
            )

        # Should be 20+ (only second retry should run)
        assert multiple_name_count >= 20, (
            f"Expected multiple name count >= 20 (second retry only), got {multiple_name_count}"
        )

        # Wait for const char retry test
        try:
            await asyncio.wait_for(const_char_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail(
                f"Const char retry test did not complete. Count: {const_char_retry_count}"
            )

        assert const_char_retry_count == 1, (
            f"Expected 1 const char retry call, got {const_char_retry_count}"
        )

        # Wait for static char retry test
        try:
            await asyncio.wait_for(static_char_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail(
                f"Static char retry test did not complete. Count: {static_char_retry_count}"
            )

        assert static_char_retry_count == 1, (
            f"Expected 1 static char retry call, got {static_char_retry_count}"
        )

        # Wait for mixed cancel test
        try:
            await asyncio.wait_for(mixed_cancel_done.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Mixed cancel test did not complete")

        assert mixed_cancel_result is True, (
            "Mixed string/const char cancel should have succeeded"
        )

        # Wait for test completion
        try:
            await asyncio.wait_for(test_complete.wait(), timeout=1.0)
        except TimeoutError:
            pytest.fail("Test did not complete within timeout")
