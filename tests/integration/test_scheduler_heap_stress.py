"""Stress test for heap scheduler thread safety with multiple threads."""

import asyncio
from pathlib import Path
import re

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_heap_stress(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that set_timeout/set_interval doesn't crash when called rapidly from multiple threads."""

    # Get the absolute path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Create a future to signal test completion
    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[None] = loop.create_future()

    # Track executed timeouts/intervals and their order
    executed_callbacks: set[int] = set()
    thread_executions: dict[
        int, list[int]
    ] = {}  # thread_id -> list of indices in execution order
    callback_types: dict[int, str] = {}  # callback_id -> "timeout" or "interval"

    def on_log_line(line: str) -> None:
        # Track all executed callbacks with thread and index info
        match = re.search(
            r"Executed (timeout|interval) (\d+) \(thread (\d+), index (\d+)\)", line
        )
        if not match:
            # Also check for the completion message
            if "All threads finished" in line and "Created 1000 callbacks" in line:
                # Give scheduler some time to execute callbacks
                pass
            return

        callback_type = match.group(1)
        callback_id = int(match.group(2))
        thread_id = int(match.group(3))
        index = int(match.group(4))

        # Only count each callback ID once (intervals might fire multiple times)
        if callback_id not in executed_callbacks:
            executed_callbacks.add(callback_id)
            callback_types[callback_id] = callback_type

        # Track execution order per thread
        if thread_id not in thread_executions:
            thread_executions[thread_id] = []

        # Only append if this is a new execution for this thread
        if index not in thread_executions[thread_id]:
            thread_executions[thread_id].append(index)

        # Check if we've executed all 1000 callbacks (0-999)
        if len(executed_callbacks) >= 1000 and not test_complete_future.done():
            test_complete_future.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-heap-stress-test"

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        run_stress_test_service: UserService | None = None
        for service in services:
            if service.name == "run_heap_stress_test":
                run_stress_test_service = service
                break

        assert run_stress_test_service is not None, (
            "run_heap_stress_test service not found"
        )

        # Call the run_heap_stress_test service to start the test
        client.execute_service(run_stress_test_service, {})

        # Wait for all callbacks to execute (should be quick, but give more time for scheduling)
        try:
            await asyncio.wait_for(test_complete_future, timeout=10.0)
        except TimeoutError:
            # Report how many we got
            missing_ids = sorted(set(range(1000)) - executed_callbacks)
            pytest.fail(
                f"Stress test timed out. Only {len(executed_callbacks)} of "
                f"1000 callbacks executed. Missing IDs: "
                f"{missing_ids[:20]}... (total missing: {len(missing_ids)})"
            )

        # Verify all callbacks executed
        assert len(executed_callbacks) == 1000, (
            f"Expected 1000 callbacks, got {len(executed_callbacks)}"
        )

        # Verify we have all IDs from 0-999
        expected_ids = set(range(1000))
        missing_ids = expected_ids - executed_callbacks
        assert not missing_ids, f"Missing callback IDs: {sorted(missing_ids)}"

        # Verify we have a mix of timeouts and intervals
        timeout_count = sum(1 for t in callback_types.values() if t == "timeout")
        interval_count = sum(1 for t in callback_types.values() if t == "interval")
        assert timeout_count > 0, "No timeouts were executed"
        assert interval_count > 0, "No intervals were executed"

        # Verify each thread executed callbacks
        for thread_id, indices in thread_executions.items():
            assert len(indices) == 100, (
                f"Thread {thread_id} executed {len(indices)} callbacks, expected 100"
            )
        # Total should be 1000 callbacks
        total_callbacks = timeout_count + interval_count
        assert total_callbacks == 1000, (
            f"Expected 1000 total callbacks but got {total_callbacks}"
        )
