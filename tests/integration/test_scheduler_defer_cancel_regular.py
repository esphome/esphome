"""Test that a deferred timeout cancels a regular timeout with the same name."""

import asyncio

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


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
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-defer-cancel-regular"

        # List services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        test_service: UserService | None = None
        for service in services:
            if service.name == "test_defer_cancels_regular":
                test_service = service
                break

        assert test_service is not None, "test_defer_cancels_regular service not found"

        # Execute the test
        client.execute_service(test_service, {})

        # Wait for test completion
        try:
            await asyncio.wait_for(test_complete_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(f"Test timed out. Log messages: {log_messages}")

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
