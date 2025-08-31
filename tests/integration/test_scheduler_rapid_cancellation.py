"""Rapid cancellation test - schedule and immediately cancel timeouts with string names."""

import asyncio
from pathlib import Path
import re

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_rapid_cancellation(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test rapid schedule/cancel cycles that might expose race conditions."""

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

    # Track test progress
    test_stats = {
        "log_count": 0,
        "errors": [],
        "summary_scheduled": None,
        "final_scheduled": 0,
        "final_executed": 0,
        "final_implicit_cancellations": 0,
    }

    def on_log_line(line: str) -> None:
        # Count log lines
        test_stats["log_count"] += 1

        # Check for errors (only ERROR level, not WARN)
        if "ERROR" in line:
            test_stats["errors"].append(line)

        # Parse summary statistics
        if "All threads completed. Scheduled:" in line:
            # Extract the scheduled count from the summary
            if match := re.search(r"Scheduled: (\d+)", line):
                test_stats["summary_scheduled"] = int(match.group(1))
        elif "Total scheduled:" in line:
            if match := re.search(r"Total scheduled: (\d+)", line):
                test_stats["final_scheduled"] = int(match.group(1))
        elif "Total executed:" in line:
            if match := re.search(r"Total executed: (\d+)", line):
                test_stats["final_executed"] = int(match.group(1))
        elif "Implicit cancellations (replaced):" in line and (
            match := re.search(r"Implicit cancellations \(replaced\): (\d+)", line)
        ):
            test_stats["final_implicit_cancellations"] = int(match.group(1))

        # Check for crash indicators
        if any(
            indicator in line.lower()
            for indicator in ["segfault", "abort", "assertion", "heap corruption"]
        ):
            if not test_complete_future.done():
                test_complete_future.set_exception(Exception(f"Crash detected: {line}"))
            return

        # Check for completion - wait for final message after all stats are logged
        if (
            "Test finished - all statistics reported" in line
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
        assert device_info.name == "sched-rapid-cancel-test"

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        run_test_service: UserService | None = None
        for service in services:
            if service.name == "run_rapid_cancellation_test":
                run_test_service = service
                break

        assert run_test_service is not None, (
            "run_rapid_cancellation_test service not found"
        )

        # Call the service to start the test
        client.execute_service(run_test_service, {})

        # Wait for test to complete with timeout
        try:
            await asyncio.wait_for(test_complete_future, timeout=10.0)
        except TimeoutError:
            pytest.fail(f"Test timed out. Stats: {test_stats}")

        # Check for any errors
        assert len(test_stats["errors"]) == 0, (
            f"Errors detected: {test_stats['errors']}"
        )

        # Check that we received log messages
        assert test_stats["log_count"] > 0, "No log messages received"

        # Check the summary line to verify all threads scheduled their operations
        assert test_stats["summary_scheduled"] == 400, (
            f"Expected summary to show 400 scheduled operations but got {test_stats['summary_scheduled']}"
        )

        # Check final statistics
        assert test_stats["final_scheduled"] == 400, (
            f"Expected final stats to show 400 scheduled but got {test_stats['final_scheduled']}"
        )

        assert test_stats["final_executed"] == 10, (
            f"Expected final stats to show 10 executed but got {test_stats['final_executed']}"
        )

        assert test_stats["final_implicit_cancellations"] == 390, (
            f"Expected final stats to show 390 implicit cancellations but got {test_stats['final_implicit_cancellations']}"
        )
