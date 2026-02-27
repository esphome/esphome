"""Test concurrent execution of wait_until and script.wait in direct automation actions."""

from __future__ import annotations

import asyncio
import re

import pytest

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
    loop = asyncio.get_running_loop()

    # Track completion counts
    test_results = {
        "wait_until": 0,
        "script_wait": 0,
        "wait_until_timeout": 0,
    }

    # Patterns for log messages
    wait_until_complete = re.compile(r"wait_until automation completed")
    script_wait_complete = re.compile(r"script\.wait automation completed")
    timeout_complete = re.compile(r"timeout automation completed")

    # Test completion futures
    test1_complete = loop.create_future()
    test2_complete = loop.create_future()
    test3_complete = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for completion messages."""
        # Test 1: wait_until concurrent execution
        if wait_until_complete.search(line):
            test_results["wait_until"] += 1
            if test_results["wait_until"] == 5 and not test1_complete.done():
                test1_complete.set_result(True)

        # Test 2: script.wait concurrent execution
        if script_wait_complete.search(line):
            test_results["script_wait"] += 1
            if test_results["script_wait"] == 5 and not test2_complete.done():
                test2_complete.set_result(True)

        # Test 3: wait_until with timeout
        if timeout_complete.search(line):
            test_results["wait_until_timeout"] += 1
            if test_results["wait_until_timeout"] == 5 and not test3_complete.done():
                test3_complete.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get services
        _, services = await client.list_entities_services()

        # Test 1: wait_until in automation - trigger 5 times rapidly
        test_service = next((s for s in services if s.name == "test_wait_until"), None)
        assert test_service is not None, "test_wait_until service not found"
        await client.execute_service(test_service, {})
        await asyncio.wait_for(test1_complete, timeout=3.0)

        # Verify Test 1: All 5 triggers should complete
        assert test_results["wait_until"] == 5, (
            f"Test 1: Expected 5 wait_until completions, got {test_results['wait_until']}"
        )

        # Test 2: script.wait in automation - trigger 5 times rapidly
        test_service = next((s for s in services if s.name == "test_script_wait"), None)
        assert test_service is not None, "test_script_wait service not found"
        await client.execute_service(test_service, {})
        await asyncio.wait_for(test2_complete, timeout=3.0)

        # Verify Test 2: All 5 triggers should complete
        assert test_results["script_wait"] == 5, (
            f"Test 2: Expected 5 script.wait completions, got {test_results['script_wait']}"
        )

        # Test 3: wait_until with timeout in automation - trigger 5 times rapidly
        test_service = next(
            (s for s in services if s.name == "test_wait_timeout"), None
        )
        assert test_service is not None, "test_wait_timeout service not found"
        await client.execute_service(test_service, {})
        await asyncio.wait_for(test3_complete, timeout=3.0)

        # Verify Test 3: All 5 triggers should timeout and complete
        assert test_results["wait_until_timeout"] == 5, (
            f"Test 3: Expected 5 timeout completions, got {test_results['wait_until_timeout']}"
        )
