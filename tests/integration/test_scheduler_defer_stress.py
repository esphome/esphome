"""Stress test for defer() thread safety with multiple threads."""

import asyncio
from pathlib import Path
import re

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_defer_stress(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that defer() doesn't crash when called rapidly from multiple threads."""

    # Get the absolute path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Create a future to signal test completion
    loop = asyncio.get_event_loop()
    test_complete_future: asyncio.Future[None] = loop.create_future()

    # Track executed defers and their order
    executed_defers: set[int] = set()
    thread_executions: dict[
        int, list[int]
    ] = {}  # thread_id -> list of indices in execution order
    fifo_violations: list[str] = []

    def on_log_line(line: str) -> None:
        # Track all executed defers with thread and index info
        match = re.search(r"Executed defer (\d+) \(thread (\d+), index (\d+)\)", line)
        if not match:
            return

        defer_id = int(match.group(1))
        thread_id = int(match.group(2))
        index = int(match.group(3))

        executed_defers.add(defer_id)

        # Track execution order per thread
        if thread_id not in thread_executions:
            thread_executions[thread_id] = []

        # Check FIFO ordering within thread
        if thread_executions[thread_id] and thread_executions[thread_id][-1] >= index:
            fifo_violations.append(
                f"Thread {thread_id}: index {index} executed after "
                f"{thread_executions[thread_id][-1]}"
            )

        thread_executions[thread_id].append(index)

        # Check if we've executed all 1000 defers (0-999)
        if len(executed_defers) == 1000 and not test_complete_future.done():
            test_complete_future.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-defer-stress-test"

        # List entities and services
        entity_info, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        run_stress_test_service: UserService | None = None
        for service in services:
            if service.name == "run_stress_test":
                run_stress_test_service = service
                break

        assert run_stress_test_service is not None, "run_stress_test service not found"

        # Call the run_stress_test service to start the test
        client.execute_service(run_stress_test_service, {})

        # Wait for all defers to execute (should be quick)
        try:
            await asyncio.wait_for(test_complete_future, timeout=5.0)
        except TimeoutError:
            # Report how many we got
            pytest.fail(
                f"Stress test timed out. Only {len(executed_defers)} of "
                f"1000 defers executed. Missing IDs: "
                f"{sorted(set(range(1000)) - executed_defers)[:10]}..."
            )

        # Verify all defers executed
        assert len(executed_defers) == 1000, (
            f"Expected 1000 defers, got {len(executed_defers)}"
        )

        # Verify we have all IDs from 0-999
        expected_ids = set(range(1000))
        missing_ids = expected_ids - executed_defers
        assert not missing_ids, f"Missing defer IDs: {sorted(missing_ids)}"

        # Verify FIFO ordering was maintained within each thread
        assert not fifo_violations, "FIFO ordering violations detected:\n" + "\n".join(
            fifo_violations[:10]
        )

        # Verify each thread executed all its defers in order
        for thread_id, indices in thread_executions.items():
            assert len(indices) == 100, (
                f"Thread {thread_id} executed {len(indices)} defers, expected 100"
            )
            # Indices should be 0-99 in ascending order
            assert indices == list(range(100)), (
                f"Thread {thread_id} executed indices out of order: {indices[:10]}..."
            )

        # If we got here without crashing and with proper ordering, the test passed
        assert True, (
            "Test completed successfully - all 1000 defers executed with "
            "FIFO ordering preserved"
        )
