"""Test that scheduler handles NULL names safely without crashing."""

import asyncio

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_scheduler_null_name(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler handles NULL names safely without crashing."""

    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "scheduler-null-name")

        # List services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        test_null_name_service = require_service(services, "test_null_name")

        # Execute the test
        await client.execute_service(test_null_name_service, {})

        await wait_or_fail(
            waiter.wait_for("Test completed successfully", timeout=20.0),
            10.0,
            "Test did not complete within timeout - likely crashed due to NULL name",
        )
