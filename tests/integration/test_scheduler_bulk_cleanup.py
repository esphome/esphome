"""Test that triggers the bulk cleanup path when to_remove_ > MAX_LOGICALLY_DELETED_ITEMS."""

import asyncio
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

CANCELLED_RE = re.compile(r"Successfully cancelled (\d+) timeouts")
REMOVED_RE = re.compile(r"Bulk cleanup triggered: removed (\d+) items")
POST_CLEANUP_RE = re.compile(r"Post-cleanup timeout \d+ executed correctly")


@pytest.mark.asyncio
async def test_scheduler_bulk_cleanup(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that bulk cleanup path is triggered when many items are cancelled."""

    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "scheduler-bulk-cleanup")

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        trigger_bulk_cleanup_service = require_service(services, "trigger_bulk_cleanup")

        # Execute the test
        await client.execute_service(trigger_bulk_cleanup_service, {})

        await wait_or_fail(
            waiter.wait_for(
                "All post-cleanup timeouts completed - test finished", timeout=20.0
            ),
            10.0,
            "Bulk cleanup test timed out",
        )

        cancelled = [
            int(match[1])
            for line in waiter.lines
            if (match := CANCELLED_RE.search(line))
        ]
        removed = [
            int(match[1]) for line in waiter.lines if (match := REMOVED_RE.search(line))
        ]
        post_cleanup_executed = sum(
            POST_CLEANUP_RE.search(line) is not None for line in waiter.lines
        )

        # Verify bulk cleanup was triggered
        assert any(count > 10 for count in cancelled), (
            "Bulk cleanup path was not triggered - MAX_LOGICALLY_DELETED_ITEMS threshold not reached"
        )

        # Verify cleanup statistics
        assert removed and removed[-1] > 10, (
            f"Expected more than 10 items removed, got {removed}"
        )

        # Verify scheduler still works after bulk cleanup
        assert post_cleanup_executed == 5, (
            f"Expected 5 post-cleanup timeouts to execute, but {post_cleanup_executed} executed"
        )
