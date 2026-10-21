"""Integration test for API conditional memory optimization with triggers and services."""

from __future__ import annotations

from aioesphomeapi import UserServiceArgType
import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_conditional_memory(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API triggers and services work correctly with conditional compilation."""
    waiter = LineWaiter()

    # Run with log monitoring
    async with run_compiled(yaml_config, line_callback=waiter.callback):
        async with api_client_connected() as client:
            # Verify device info
            await assert_device_name(client, "api-conditional-memory-test")

            # Wait for connection log
            await waiter.wait_for("Client", "connected from", timeout=5.0)

            # List services
            _, services = await client.list_entities_services()

            # Verify services exist
            assert len(services) == 2, f"Expected 2 services, found {len(services)}"

            # Find our services
            simple_service = require_service(services, "test_simple_service")
            service_with_args = require_service(services, "test_service_with_args")

            # Verify service arguments
            assert len(service_with_args.args) == 4, (
                f"Expected 4 args, found {len(service_with_args.args)}"
            )

            # Check arg types
            arg_types = {arg.name: arg.type for arg in service_with_args.args}
            assert arg_types["arg_string"] == UserServiceArgType.STRING
            assert arg_types["arg_int"] == UserServiceArgType.INT
            assert arg_types["arg_bool"] == UserServiceArgType.BOOL
            assert arg_types["arg_float"] == UserServiceArgType.FLOAT

            # Call simple service
            await client.execute_service(simple_service, {})

            # Wait for service log
            await waiter.wait_for("Simple service called", timeout=5.0)

            # Call service with arguments
            await client.execute_service(
                service_with_args,
                {
                    "arg_string": "test_string",
                    "arg_int": 123,
                    "arg_bool": True,
                    "arg_float": 42.5,
                },
            )

            # Wait for service with args log
            await waiter.wait_for(
                "Service called with: test_string, 123, 1, 42.50", timeout=5.0
            )

        # Client disconnected here, wait for disconnect log
        await waiter.wait_for("Client", "disconnected from", timeout=5.0)

        # Verify fix for issue #11131: api.connected should be false in trigger
        # when the last client disconnects
        await waiter.wait_for("No clients remaining", timeout=5.0)
