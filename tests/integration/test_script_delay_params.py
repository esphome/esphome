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

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


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
    waiter = LineWaiter()

    def iterations(pattern: str) -> set[int]:
        return {
            int(m.group(1)) for line in waiter.lines if (m := re.search(pattern, line))
        }

    def wait_returned() -> set[int]:
        return iterations(r"Father iteration (\d+): son finished, wait returned")

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "test-script-delay-params")

        # Get services
        _, services = await client.list_entities_services()
        test_service = require_service(services, "test_repeat_with_delay")

        # Execute the test
        await client.execute_service(test_service, {})

        # Wait for test to complete (10 iterations * ~100ms each + margin)
        await wait_or_fail(
            waiter.wait_for("Father iteration 9: son finished, wait returned"),
            5.0,
            "Test timed out. "
            "This likely indicates the script became a zombie (issue #12044).",
            wait_returned=wait_returned,
        )

        # Verify all 10 iterations completed successfully
        expected_iterations = set(range(10))
        father_calling = iterations(r"Father iteration (\d+): calling son")
        son_started = iterations(r"Son script started with iteration (\d+)")
        son_delaying = iterations(r"Son script delaying for iteration (\d+)")
        son_finished = iterations(r"Son script finished with iteration (\d+)")
        assert father_calling == expected_iterations, "Not all iterations started"
        assert son_started == expected_iterations, (
            "Son script not started for all iterations"
        )
        assert son_finished == expected_iterations, (
            "Son script not finished for all iterations"
        )
        assert wait_returned() == expected_iterations, (
            "script.wait did not return for all iterations"
        )

        # Verify delays were triggered for iterations >= 5
        expected_delays = set(range(5, 10))
        assert son_delaying == expected_delays, (
            "Delays not triggered for iterations >= 5"
        )
