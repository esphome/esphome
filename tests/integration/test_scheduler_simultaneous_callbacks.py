"""Simultaneous callbacks test - schedule many callbacks for the same time from multiple threads."""

import asyncio
import re

import pytest

from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_simultaneous_callbacks(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test scheduling many callbacks for the exact same time from multiple threads."""

    # Create a future to signal test completion
    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[None] = loop.create_future()

    # Track test progress
    test_stats = {
        "scheduled": 0,
        "executed": 0,
        "expected": 1000,  # 10 threads * 100 callbacks
        "errors": [],
    }

    def on_log_line(line: str) -> None:
        # Track operations
        if "Scheduled callback" in line:
            test_stats["scheduled"] += 1
        elif "Callback executed" in line:
            test_stats["executed"] += 1
        elif "ERROR" in line:
            test_stats["errors"].append(line)

        # Check for crash indicators
        if any(
            indicator in line.lower()
            for indicator in ["segfault", "abort", "assertion", "heap corruption"]
        ):
            if not test_complete_future.done():
                test_complete_future.set_exception(Exception(f"Crash detected: {line}"))
            return

        # Check for completion with final count
        if "Final executed count:" in line:
            # Extract number from log line like: "[07:59:47][I][simultaneous_callbacks:093]: Simultaneous callbacks test complete. Final executed count: 1000"
            match = re.search(r"Final executed count:\s*(\d+)", line)
            if match:
                test_stats["final_count"] = int(match.group(1))

        # Check for completion
        if (
            "Simultaneous callbacks test complete" in line
            and not test_complete_future.done()
        ):
            test_complete_future.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "sched-simul-callbacks-test"

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        run_test_service = require_service(services, "run_simultaneous_callbacks_test")

        # Call the service to start the test
        await client.execute_service(run_test_service, {})

        # Wait for test to complete
        try:
            await asyncio.wait_for(test_complete_future, timeout=30.0)
        except TimeoutError:
            pytest.fail(f"Simultaneous callbacks test timed out. Stats: {test_stats}")

        # Check for any errors
        assert len(test_stats["errors"]) == 0, (
            f"Errors detected: {test_stats['errors']}"
        )

        # Verify all callbacks executed using the final count from C++
        final_count = test_stats.get("final_count", 0)
        assert final_count == test_stats["expected"], (
            f"Expected {test_stats['expected']} callbacks, but only {final_count} executed"
        )

        # The final_count is the authoritative count from the C++ component
        assert final_count == 1000, (
            f"Expected 1000 executed callbacks but got {final_count}"
        )
