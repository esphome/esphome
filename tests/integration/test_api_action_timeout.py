"""Integration test for API action call timeout functionality.

Tests that action calls are automatically cleaned up after timeout,
and that late responses are handled gracefully.
"""

from __future__ import annotations

import asyncio
import contextlib
import re

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_action_timeout(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API action call timeout behavior.

    This test uses a 500ms timeout (set via USE_API_ACTION_CALL_TIMEOUT_MS define)
    to verify:
    1. Actions that respond within the timeout work correctly
    2. Actions that exceed the timeout have their calls cleaned up
    3. Late responses log a warning but don't crash
    """
    loop = asyncio.get_running_loop()

    # Track log messages
    immediate_future = loop.create_future()
    short_delay_responding_future = loop.create_future()
    long_delay_starting_future = loop.create_future()
    long_delay_responding_future = loop.create_future()
    timeout_warning_future = loop.create_future()

    # Patterns to match in logs
    immediate_pattern = re.compile(r"ACTION_IMMEDIATE responding")
    short_delay_responding_pattern = re.compile(r"ACTION_SHORT_DELAY responding")
    long_delay_starting_pattern = re.compile(r"ACTION_LONG_DELAY starting")
    long_delay_responding_pattern = re.compile(
        r"ACTION_LONG_DELAY responding \(after timeout\)"
    )
    # This warning is logged when api.respond is called after the action call timed out
    timeout_warning_pattern = re.compile(
        r"Cannot send response: no active call found for action_call_id"
    )

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not immediate_future.done() and immediate_pattern.search(line):
            immediate_future.set_result(True)
        elif (
            not short_delay_responding_future.done()
            and short_delay_responding_pattern.search(line)
        ):
            short_delay_responding_future.set_result(True)
        elif (
            not long_delay_starting_future.done()
            and long_delay_starting_pattern.search(line)
        ):
            long_delay_starting_future.set_result(True)
        elif (
            not long_delay_responding_future.done()
            and long_delay_responding_pattern.search(line)
        ):
            long_delay_responding_future.set_result(True)
        elif not timeout_warning_future.done() and timeout_warning_pattern.search(line):
            timeout_warning_future.set_result(True)

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "api-action-timeout-test"

        # List services
        _, services = await client.list_entities_services()

        # Should have 3 services
        assert len(services) == 3, f"Expected 3 services, found {len(services)}"

        # Find our services
        action_immediate: UserService | None = None
        action_short_delay: UserService | None = None
        action_long_delay: UserService | None = None

        for service in services:
            if service.name == "action_immediate":
                action_immediate = service
            elif service.name == "action_short_delay":
                action_short_delay = service
            elif service.name == "action_long_delay":
                action_long_delay = service

        assert action_immediate is not None, "action_immediate not found"
        assert action_short_delay is not None, "action_short_delay not found"
        assert action_long_delay is not None, "action_long_delay not found"

        # Test 1: Immediate response should work
        response = await client.execute_service(
            action_immediate,
            {},
            return_response=True,
        )
        await asyncio.wait_for(immediate_future, timeout=1.0)
        assert response is not None, "Expected response for immediate action"
        assert response.success is True

        # Test 2: Short delay (200ms) should work within the 500ms timeout
        response = await client.execute_service(
            action_short_delay,
            {},
            return_response=True,
        )
        await asyncio.wait_for(short_delay_responding_future, timeout=1.0)
        assert response is not None, "Expected response for short delay action"
        assert response.success is True

        # Test 3: Long delay (1s) should exceed the 500ms timeout
        # The server-side timeout will clean up the action call after 500ms
        # The client will timeout waiting for the response
        # When the action finally tries to respond after 1s, it will log a warning

        # Start the long delay action (don't await it fully - it will timeout)
        long_delay_task = asyncio.create_task(
            client.execute_service(
                action_long_delay,
                {},
                return_response=True,
                timeout=2.0,  # Give client enough time to see the late response attempt
            )
        )

        # Wait for the action to start
        await asyncio.wait_for(long_delay_starting_future, timeout=1.0)

        # Wait for the action to try to respond (after 1s delay)
        await asyncio.wait_for(long_delay_responding_future, timeout=2.0)

        # Wait for the warning log about no active call
        await asyncio.wait_for(timeout_warning_future, timeout=1.0)

        # The client task should complete (either with None response or timeout)
        # Client timing out is acceptable - the server-side timeout already cleaned up the call
        with contextlib.suppress(TimeoutError):
            await asyncio.wait_for(long_delay_task, timeout=1.0)

        # Verify the system is still functional after the timeout
        # Call the immediate action again to prove cleanup worked
        immediate_future_2 = loop.create_future()

        def check_output_2(line: str) -> None:
            if not immediate_future_2.done() and immediate_pattern.search(line):
                immediate_future_2.set_result(True)

        response = await client.execute_service(
            action_immediate,
            {},
            return_response=True,
        )
        assert response is not None, "System should still work after timeout"
        assert response.success is True
