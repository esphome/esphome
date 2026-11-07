"""Integration test for API action call timeout functionality.

Tests that action calls are automatically cleaned up after timeout,
and that late responses are handled gracefully.
"""

from __future__ import annotations

import asyncio
import contextlib

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
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
    waiter = LineWaiter()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "api-action-timeout-test")

        # List services
        _, services = await client.list_entities_services()

        # Should have 3 services
        assert len(services) == 3, f"Expected 3 services, found {len(services)}"

        # Find our services
        action_immediate = require_service(services, "action_immediate")
        action_short_delay = require_service(services, "action_short_delay")
        action_long_delay = require_service(services, "action_long_delay")

        # Test 1: Immediate response should work
        response = await client.execute_service(
            action_immediate,
            {},
            return_response=True,
        )
        await waiter.wait_for("ACTION_IMMEDIATE responding", timeout=1.0)
        assert response is not None, "Expected response for immediate action"
        assert response.success is True

        # Test 2: Short delay (200ms) should work within the 500ms timeout
        response = await client.execute_service(
            action_short_delay,
            {},
            return_response=True,
        )
        await waiter.wait_for("ACTION_SHORT_DELAY responding", timeout=1.0)
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
        await waiter.wait_for("ACTION_LONG_DELAY starting", timeout=1.0)

        # Wait for the action to try to respond (after 1s delay)
        await waiter.wait_for(
            "ACTION_LONG_DELAY responding (after timeout)", timeout=2.0
        )

        # Wait for the warning log about no active call
        await waiter.wait_for(
            "Cannot send response: no active call found for action_call_id",
            timeout=1.0,
        )

        # The client task should complete (either with None response or timeout)
        # Client timing out is acceptable - the server-side timeout already cleaned up the call
        with contextlib.suppress(TimeoutError):
            await asyncio.wait_for(long_delay_task, timeout=1.0)

        # Verify the system is still functional after the timeout
        # Call the immediate action again to prove cleanup worked
        response = await client.execute_service(
            action_immediate,
            {},
            return_response=True,
        )
        assert response is not None, "System should still work after timeout"
        assert response.success is True
