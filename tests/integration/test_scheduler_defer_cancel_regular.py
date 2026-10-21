"""Test that a deferred timeout cancels a regular timeout with the same name."""

import asyncio

import pytest

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

    # Create a future to signal test completion
    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[None] = loop.create_future()

    # Track log messages
    log_messages: list[str] = []
    error_detected = False

    def on_log_line(line: str) -> None:
        nonlocal error_detected
        if "TEST" in line:
            log_messages.append(line)

        if "ERROR: Regular timeout executed" in line:
            error_detected = True

        if "Test complete" in line and not test_complete_future.done():
            test_complete_future.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
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

        # Wait for test completion
        await wait_or_fail(
            test_complete_future, 5.0, "Test timed out", log_messages=log_messages
        )

        # Verify results
        assert not error_detected, (
            f"Regular timeout should have been cancelled but it executed! Logs: {log_messages}"
        )

        # Verify the deferred timeout executed
        assert any(
            "SUCCESS: Deferred timeout executed" in msg for msg in log_messages
        ), f"Deferred timeout should have executed. Logs: {log_messages}"

        # Verify the expected sequence of events
        assert any(
            "Starting defer cancels regular timeout test" in msg for msg in log_messages
        )
        assert any(
            "Scheduled regular timeout with 100ms delay" in msg for msg in log_messages
        )
        assert any(
            "Scheduled deferred timeout - should cancel regular timeout" in msg
            for msg in log_messages
        )
