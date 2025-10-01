"""Test for scheduler race condition where removed items still execute."""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_removed_item_race(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that items marked for removal don't execute.

    This test verifies the fix for a race condition where:
    1. cleanup_() only removes items from the front of the heap
    2. Items in the middle of the heap marked for removal still execute
    3. This causes cancelled timeouts to run when they shouldn't
    """

    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[bool] = loop.create_future()

    # Track test results
    test_passed = False
    removed_executed = 0
    normal_executed = 0

    # Patterns to match
    race_pattern = re.compile(r"RACE: .* executed after being cancelled!")
    passed_pattern = re.compile(r"TEST PASSED")
    failed_pattern = re.compile(r"TEST FAILED")
    complete_pattern = re.compile(r"=== Test Complete ===")
    normal_count_pattern = re.compile(r"Normal items executed: (\d+)")
    removed_count_pattern = re.compile(r"Removed items executed: (\d+)")

    def check_output(line: str) -> None:
        """Check log output for test results."""
        nonlocal test_passed, removed_executed, normal_executed

        if race_pattern.search(line):
            # Race condition detected - a cancelled item executed
            test_passed = False

        if passed_pattern.search(line):
            test_passed = True
        elif failed_pattern.search(line):
            test_passed = False

        normal_match = normal_count_pattern.search(line)
        if normal_match:
            normal_executed = int(normal_match.group(1))

        removed_match = removed_count_pattern.search(line)
        if removed_match:
            removed_executed = int(removed_match.group(1))

        if not test_complete_future.done() and complete_pattern.search(line):
            test_complete_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-removed-item-race"

        # List services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find run_test service
        run_test_service = next((s for s in services if s.name == "run_test"), None)
        assert run_test_service is not None, "run_test service not found"

        # Execute the test
        client.execute_service(run_test_service, {})

        # Wait for test completion
        try:
            await asyncio.wait_for(test_complete_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Test did not complete within timeout")

        # Verify results
        assert test_passed, (
            f"Test failed! Removed items executed: {removed_executed}, "
            f"Normal items executed: {normal_executed}"
        )
        assert removed_executed == 0, (
            f"Cancelled items should not execute, but {removed_executed} did"
        )
        assert normal_executed == 4, (
            f"Expected 4 normal items to execute, got {normal_executed}"
        )
