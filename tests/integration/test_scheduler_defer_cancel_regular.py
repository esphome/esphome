"""Test that a deferred timeout cancels a regular timeout with the same name."""

import asyncio

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_scheduler_defer_cancels_regular(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that set_timeout(name, 0) cancels a previously scheduled set_timeout(name, delay)."""

    waiter = LineWaiter()

    def log_messages() -> list[str]:
        return [line for line in waiter.lines if "TEST" in line]

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "scheduler-defer-cancel-regular")

        # List services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        test_service = require_service(services, "test_defer_cancels_regular")

        # Execute the test
        await client.execute_service(test_service, {})

        await wait_or_fail(
            waiter.wait_for("Test complete", timeout=20.0),
            5.0,
            "Test timed out",
            log_messages=log_messages,
        )

        messages = log_messages()
        assert not any(
            "ERROR: Regular timeout executed" in line for line in waiter.lines
        ), (
            f"Regular timeout should have been cancelled but it executed! Logs: {messages}"
        )

        # Verify the deferred timeout executed
        assert any("SUCCESS: Deferred timeout executed" in msg for msg in messages), (
            f"Deferred timeout should have executed. Logs: {messages}"
        )

        # Verify the expected sequence of events
        assert any(
            "Starting defer cancels regular timeout test" in msg for msg in messages
        )
        assert any(
            "Scheduled regular timeout with 100ms delay" in msg for msg in messages
        )
        assert any(
            "Scheduled deferred timeout - should cancel regular timeout" in msg
            for msg in messages
        )
