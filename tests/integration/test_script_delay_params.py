"""Integration test for script.wait FIFO ordering (issues #12043, #12044).

This test verifies that ScriptWaitAction processes queued items in FIFO order.

PR #7972 introduced bugs in ScriptWaitAction:
- Used emplace_front() causing LIFO ordering instead of FIFO
- Called loop() synchronously causing reentrancy issues
- Used while loop processing entire queue causing infinite loops

These bugs manifested as:
- Scripts becoming "zombies" (stuck in running state)
- script.wait hanging forever
- Incorrect execution order
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_delay_with_params(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that script.wait processes queued items in FIFO order.

    This reproduces issues #12043 and #12044 where scripts would hang or become
    zombies due to LIFO ordering bugs in ScriptWaitAction from PR #7972.
    """
    test_complete = asyncio.Event()

    # Patterns to match in logs
    father_calling_pattern = re.compile(r"Father iteration (\d+): calling son")
    son_started_pattern = re.compile(r"Son script started with iteration (\d+)")
    son_delaying_pattern = re.compile(r"Son script delaying for iteration (\d+)")
    son_finished_pattern = re.compile(r"Son script finished with iteration (\d+)")
    father_wait_returned_pattern = re.compile(
        r"Father iteration (\d+): son finished, wait returned"
    )

    # Track which iterations completed
    father_calling = set()
    son_started = set()
    son_delaying = set()
    son_finished = set()
    wait_returned = set()

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if test_complete.is_set():
            return

        if mo := father_calling_pattern.search(line):
            father_calling.add(int(mo.group(1)))
        elif mo := son_started_pattern.search(line):
            son_started.add(int(mo.group(1)))
        elif mo := son_delaying_pattern.search(line):
            son_delaying.add(int(mo.group(1)))
        elif mo := son_finished_pattern.search(line):
            son_finished.add(int(mo.group(1)))
        elif mo := father_wait_returned_pattern.search(line):
            iteration = int(mo.group(1))
            wait_returned.add(iteration)
            # Test completes when iteration 9 finishes
            if iteration == 9:
                test_complete.set()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "test-script-delay-params"

        # Get services
        _, services = await client.list_entities_services()
        test_service = next(
            (s for s in services if s.name == "test_repeat_with_delay"), None
        )
        assert test_service is not None, "test_repeat_with_delay service not found"

        # Execute the test
        client.execute_service(test_service, {})

        # Wait for test to complete (10 iterations * ~100ms each + margin)
        try:
            await asyncio.wait_for(test_complete.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Test timed out. Completed iterations: {sorted(wait_returned)}. "
                f"This likely indicates the script became a zombie (issue #12044)."
            )

        # Verify all 10 iterations completed successfully
        expected_iterations = set(range(10))
        assert father_calling == expected_iterations, "Not all iterations started"
        assert son_started == expected_iterations, (
            "Son script not started for all iterations"
        )
        assert son_finished == expected_iterations, (
            "Son script not finished for all iterations"
        )
        assert wait_returned == expected_iterations, (
            "script.wait did not return for all iterations"
        )

        # Verify delays were triggered for iterations >= 5
        expected_delays = set(range(5, 10))
        assert son_delaying == expected_delays, (
            "Delays not triggered for iterations >= 5"
        )
