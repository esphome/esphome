"""Test that triggers the bulk cleanup path when to_remove_ > MAX_LOGICALLY_DELETED_ITEMS."""

import asyncio
from pathlib import Path
import re

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_bulk_cleanup(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that bulk cleanup path is triggered when many items are cancelled."""

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
    bulk_cleanup_triggered = False
    cleanup_stats: dict[str, int] = {
        "removed": 0,
        "before": 0,
        "after": 0,
    }
    post_cleanup_executed = 0

    def on_log_line(line: str) -> None:
        nonlocal bulk_cleanup_triggered, post_cleanup_executed

        # Look for logs indicating bulk cleanup was triggered
        # The actual cleanup happens silently, so we track the cancel operations
        if "Successfully cancelled" in line and "timeouts" in line:
            match = re.search(r"Successfully cancelled (\d+) timeouts", line)
            if match and int(match.group(1)) > 10:
                bulk_cleanup_triggered = True

        # Track cleanup statistics
        match = re.search(r"Bulk cleanup triggered: removed (\d+) items", line)
        if match:
            cleanup_stats["removed"] = int(match.group(1))

        match = re.search(r"Items before cleanup: (\d+), after: (\d+)", line)
        if match:
            cleanup_stats["before"] = int(match.group(1))
            cleanup_stats["after"] = int(match.group(2))

        # Track post-cleanup timeout executions
        if "Post-cleanup timeout" in line and "executed correctly" in line:
            match = re.search(r"Post-cleanup timeout (\d+) executed correctly", line)
            if match:
                post_cleanup_executed += 1

        # Check for final test completion
        if (
            "All post-cleanup timeouts completed - test finished" in line
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
        assert device_info.name == "scheduler-bulk-cleanup"

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        trigger_bulk_cleanup_service: UserService | None = None
        for service in services:
            if service.name == "trigger_bulk_cleanup":
                trigger_bulk_cleanup_service = service
                break

        assert trigger_bulk_cleanup_service is not None, (
            "trigger_bulk_cleanup service not found"
        )

        # Execute the test
        client.execute_service(trigger_bulk_cleanup_service, {})

        # Wait for test completion
        try:
            await asyncio.wait_for(test_complete_future, timeout=10.0)
        except TimeoutError:
            pytest.fail("Bulk cleanup test timed out")

        # Verify bulk cleanup was triggered
        assert bulk_cleanup_triggered, (
            "Bulk cleanup path was not triggered - MAX_LOGICALLY_DELETED_ITEMS threshold not reached"
        )

        # Verify cleanup statistics
        assert cleanup_stats["removed"] > 10, (
            f"Expected more than 10 items removed, got {cleanup_stats['removed']}"
        )

        # Verify scheduler still works after bulk cleanup
        assert post_cleanup_executed == 5, (
            f"Expected 5 post-cleanup timeouts to execute, but {post_cleanup_executed} executed"
        )
