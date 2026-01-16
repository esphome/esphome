"""Integration test for API string arguments in sync/async services.

Tests that:
1. Synchronous services use zero-copy StringRef and .compare() works
2. Asynchronous services (with delay) use std::string copy safely
3. Multiple string arguments work correctly
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import UserService, UserServiceArgType
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_string_sync_async(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that string arguments work in both sync and async services."""
    loop = asyncio.get_running_loop()

    # Track log messages
    sync_start_future = loop.create_future()
    sync_stop_future = loop.create_future()
    sync_unknown_future = loop.create_future()
    async_before_future = loop.create_future()
    async_after_future = loop.create_future()
    multi_string_future = loop.create_future()

    # Patterns to match in logs
    sync_start_pattern = re.compile(r"Sync command: start")
    sync_stop_pattern = re.compile(r"Sync command: stop")
    sync_unknown_pattern = re.compile(r"Sync unknown command: custom_cmd")
    async_before_pattern = re.compile(r"Async before delay: test_message")
    async_after_pattern = re.compile(r"Async after delay: test_message")
    # The output uses %d for bool which prints 0/1
    multi_string_pattern = re.compile(r"Multi string: first=hello.*second=world")

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not sync_start_future.done() and sync_start_pattern.search(line):
            sync_start_future.set_result(True)
        elif not sync_stop_future.done() and sync_stop_pattern.search(line):
            sync_stop_future.set_result(True)
        elif not sync_unknown_future.done() and sync_unknown_pattern.search(line):
            sync_unknown_future.set_result(True)
        elif not async_before_future.done() and async_before_pattern.search(line):
            async_before_future.set_result(True)
        elif not async_after_future.done() and async_after_pattern.search(line):
            async_after_future.set_result(True)
        elif not multi_string_future.done() and multi_string_pattern.search(line):
            multi_string_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "api-string-sync-async-test"

        # List services
        _, services = await client.list_entities_services()

        # Should have 3 services
        assert len(services) == 3, f"Expected 3 services, found {len(services)}"

        # Find our services
        sync_service: UserService | None = None
        async_service: UserService | None = None
        multi_service: UserService | None = None

        for service in services:
            if service.name == "sync_string_service":
                sync_service = service
            elif service.name == "async_string_service":
                async_service = service
            elif service.name == "sync_multi_string":
                multi_service = service

        assert sync_service is not None, "sync_string_service not found"
        assert async_service is not None, "async_string_service not found"
        assert multi_service is not None, "sync_multi_string not found"

        # Verify service arguments
        assert len(sync_service.args) == 1
        assert sync_service.args[0].name == "command"
        assert sync_service.args[0].type == UserServiceArgType.STRING

        assert len(async_service.args) == 1
        assert async_service.args[0].name == "message"
        assert async_service.args[0].type == UserServiceArgType.STRING

        assert len(multi_service.args) == 2
        multi_arg_types = {arg.name: arg.type for arg in multi_service.args}
        assert multi_arg_types["first"] == UserServiceArgType.STRING
        assert multi_arg_types["second"] == UserServiceArgType.STRING

        # Test sync service with "start" command (tests .compare())
        await client.execute_service(sync_service, {"command": "start"})
        await asyncio.wait_for(sync_start_future, timeout=5.0)

        # Test sync service with "stop" command
        await client.execute_service(sync_service, {"command": "stop"})
        await asyncio.wait_for(sync_stop_future, timeout=5.0)

        # Test sync service with unknown command (tests .c_str())
        await client.execute_service(sync_service, {"command": "custom_cmd"})
        await asyncio.wait_for(sync_unknown_future, timeout=5.0)

        # Test async service - this has a delay so needs std::string copy
        await client.execute_service(async_service, {"message": "test_message"})
        await asyncio.wait_for(async_before_future, timeout=5.0)
        # Wait for the delayed log (100ms delay + some margin)
        await asyncio.wait_for(async_after_future, timeout=5.0)

        # Test multi-string service
        await client.execute_service(
            multi_service, {"first": "hello", "second": "world"}
        )
        await asyncio.wait_for(multi_string_future, timeout=5.0)
