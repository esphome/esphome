"""Test concurrent execution of wait_until and script.wait in direct automation actions."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_automation_wait_actions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """
    Test that wait_until and script.wait correctly handle concurrent executions
    when automation actions (not scripts) are triggered multiple times rapidly.

    This tests sensor.on_value automations being triggered 5 times before any complete.
    """
    waiter = LineWaiter()

    # Patterns for log messages
    wait_until_complete = "wait_until automation completed"
    script_wait_complete = "script.wait automation completed"
    timeout_complete = "timeout automation completed"

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Get services
        _, services = await client.list_entities_services()

        # Test 1: wait_until in automation - trigger 5 times rapidly
        test_service = require_service(services, "test_wait_until")
        await client.execute_service(test_service, {})
        await waiter.wait_count(wait_until_complete, 5, 3.0, "Test 1 did not complete")

        # Verify Test 1: All 5 triggers should complete
        completions = waiter.count(wait_until_complete)
        assert completions == 5, (
            f"Test 1: Expected 5 wait_until completions, got {completions}"
        )

        # Test 2: script.wait in automation - trigger 5 times rapidly
        test_service = require_service(services, "test_script_wait")
        await client.execute_service(test_service, {})
        await waiter.wait_count(script_wait_complete, 5, 3.0, "Test 2 did not complete")

        # Verify Test 2: All 5 triggers should complete
        completions = waiter.count(script_wait_complete)
        assert completions == 5, (
            f"Test 2: Expected 5 script.wait completions, got {completions}"
        )

        # Test 3: wait_until with timeout in automation - trigger 5 times rapidly
        test_service = require_service(services, "test_wait_timeout")
        await client.execute_service(test_service, {})
        await waiter.wait_count(timeout_complete, 5, 3.0, "Test 3 did not complete")

        # Verify Test 3: All 5 triggers should timeout and complete
        completions = waiter.count(timeout_complete)
        assert completions == 5, (
            f"Test 3: Expected 5 timeout completions, got {completions}"
        )
