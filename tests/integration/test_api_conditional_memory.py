"""Integration test for API conditional memory optimization with triggers and services."""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import UserService, UserServiceArgType
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_conditional_memory(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API triggers and services work correctly with conditional compilation."""
    loop = asyncio.get_running_loop()

    # Track log messages
    connected_future = loop.create_future()
    disconnected_future = loop.create_future()
    service_simple_future = loop.create_future()
    service_args_future = loop.create_future()

    # Patterns to match in logs
    connected_pattern = re.compile(r"Client .* connected from")
    disconnected_pattern = re.compile(r"Client .* disconnected from")
    service_simple_pattern = re.compile(r"Simple service called")
    service_args_pattern = re.compile(
        r"Service called with: test_string, 123, 1, 42\.50"
    )

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not connected_future.done() and connected_pattern.search(line):
            connected_future.set_result(True)
        elif not disconnected_future.done() and disconnected_pattern.search(line):
            disconnected_future.set_result(True)
        elif not service_simple_future.done() and service_simple_pattern.search(line):
            service_simple_future.set_result(True)
        elif not service_args_future.done() and service_args_pattern.search(line):
            service_args_future.set_result(True)

    # Run with log monitoring
    async with run_compiled(yaml_config, line_callback=check_output):
        async with api_client_connected() as client:
            # Verify device info
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "api-conditional-memory-test"

            # Wait for connection log
            await asyncio.wait_for(connected_future, timeout=5.0)

            # List services
            _, services = await client.list_entities_services()

            # Verify services exist
            assert len(services) == 2, f"Expected 2 services, found {len(services)}"

            # Find our services
            simple_service: UserService | None = None
            service_with_args: UserService | None = None

            for service in services:
                if service.name == "test_simple_service":
                    simple_service = service
                elif service.name == "test_service_with_args":
                    service_with_args = service

            assert simple_service is not None, "test_simple_service not found"
            assert service_with_args is not None, "test_service_with_args not found"

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
            await asyncio.wait_for(service_simple_future, timeout=5.0)

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
            await asyncio.wait_for(service_args_future, timeout=5.0)

        # Client disconnected here, wait for disconnect log
        await asyncio.wait_for(disconnected_future, timeout=5.0)
