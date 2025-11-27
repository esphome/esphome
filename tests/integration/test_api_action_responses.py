"""Integration test for API action responses feature.

Tests the supports_response modes: none, status, optional, only.
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import UserService, UserServiceArgType
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_action_responses(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API action response modes work correctly."""
    loop = asyncio.get_running_loop()

    # Track log messages for each action type
    no_response_future = loop.create_future()
    status_success_future = loop.create_future()
    status_error_future = loop.create_future()
    optional_response_future = loop.create_future()
    only_response_future = loop.create_future()
    nested_json_future = loop.create_future()

    # Patterns to match in logs
    no_response_pattern = re.compile(r"ACTION_NO_RESPONSE called with: test_message")
    status_success_pattern = re.compile(
        r"ACTION_STATUS_RESPONSE success \(call_id=\d+\)"
    )
    status_error_pattern = re.compile(r"ACTION_STATUS_RESPONSE error \(call_id=\d+\)")
    optional_response_pattern = re.compile(
        r"ACTION_OPTIONAL_RESPONSE \(call_id=\d+, return_response=\d+, value=42\)"
    )
    only_response_pattern = re.compile(
        r"ACTION_ONLY_RESPONSE \(call_id=\d+, name=World\)"
    )
    nested_json_pattern = re.compile(r"ACTION_NESTED_JSON \(call_id=\d+\)")

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not no_response_future.done() and no_response_pattern.search(line):
            no_response_future.set_result(True)
        elif not status_success_future.done() and status_success_pattern.search(line):
            status_success_future.set_result(True)
        elif not status_error_future.done() and status_error_pattern.search(line):
            status_error_future.set_result(True)
        elif not optional_response_future.done() and optional_response_pattern.search(
            line
        ):
            optional_response_future.set_result(True)
        elif not only_response_future.done() and only_response_pattern.search(line):
            only_response_future.set_result(True)
        elif not nested_json_future.done() and nested_json_pattern.search(line):
            nested_json_future.set_result(True)

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "api-action-responses-test"

        # List services
        _, services = await client.list_entities_services()

        # Should have 5 services
        assert len(services) == 5, f"Expected 5 services, found {len(services)}"

        # Find our services
        action_no_response: UserService | None = None
        action_status_response: UserService | None = None
        action_optional_response: UserService | None = None
        action_only_response: UserService | None = None
        action_nested_json: UserService | None = None

        for service in services:
            if service.name == "action_no_response":
                action_no_response = service
            elif service.name == "action_status_response":
                action_status_response = service
            elif service.name == "action_optional_response":
                action_optional_response = service
            elif service.name == "action_only_response":
                action_only_response = service
            elif service.name == "action_nested_json":
                action_nested_json = service

        assert action_no_response is not None, "action_no_response not found"
        assert action_status_response is not None, "action_status_response not found"
        assert action_optional_response is not None, (
            "action_optional_response not found"
        )
        assert action_only_response is not None, "action_only_response not found"
        assert action_nested_json is not None, "action_nested_json not found"

        # Verify argument types
        # action_no_response: string message
        assert len(action_no_response.args) == 1
        assert action_no_response.args[0].name == "message"
        assert action_no_response.args[0].type == UserServiceArgType.STRING

        # action_status_response: bool should_succeed
        assert len(action_status_response.args) == 1
        assert action_status_response.args[0].name == "should_succeed"
        assert action_status_response.args[0].type == UserServiceArgType.BOOL

        # action_optional_response: int value
        assert len(action_optional_response.args) == 1
        assert action_optional_response.args[0].name == "value"
        assert action_optional_response.args[0].type == UserServiceArgType.INT

        # action_only_response: string name
        assert len(action_only_response.args) == 1
        assert action_only_response.args[0].name == "name"
        assert action_only_response.args[0].type == UserServiceArgType.STRING

        # action_nested_json: no args
        assert len(action_nested_json.args) == 0

        # Test action_no_response (supports_response: none)
        client.execute_service(action_no_response, {"message": "test_message"})
        await asyncio.wait_for(no_response_future, timeout=5.0)

        # Test action_status_response with success (supports_response: status)
        client.execute_service(action_status_response, {"should_succeed": True})
        await asyncio.wait_for(status_success_future, timeout=5.0)

        # Test action_status_response with error
        client.execute_service(action_status_response, {"should_succeed": False})
        await asyncio.wait_for(status_error_future, timeout=5.0)

        # Test action_optional_response (supports_response: optional)
        client.execute_service(action_optional_response, {"value": 42})
        await asyncio.wait_for(optional_response_future, timeout=5.0)

        # Test action_only_response (supports_response: only)
        client.execute_service(action_only_response, {"name": "World"})
        await asyncio.wait_for(only_response_future, timeout=5.0)

        # Test action_nested_json
        client.execute_service(action_nested_json, {})
        await asyncio.wait_for(nested_json_future, timeout=5.0)

        # All services were called successfully and produced expected log output
        # Note: Response data verification requires aioesphomeapi to be updated
        # to support the supports_response field and response callbacks
