"""Integration test for API action responses feature.

Tests the supports_response modes: none, status, optional, only.
"""

from __future__ import annotations

import asyncio
import json
import re

from aioesphomeapi import SupportsResponseType, UserService, UserServiceArgType
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

        # Verify supports_response modes
        assert action_no_response.supports_response is None or (
            action_no_response.supports_response == SupportsResponseType.NONE
        ), (
            f"action_no_response should have supports_response=NONE, got {action_no_response.supports_response}"
        )

        assert (
            action_status_response.supports_response == SupportsResponseType.STATUS
        ), (
            f"action_status_response should have supports_response=STATUS, "
            f"got {action_status_response.supports_response}"
        )

        assert (
            action_optional_response.supports_response == SupportsResponseType.OPTIONAL
        ), (
            f"action_optional_response should have supports_response=OPTIONAL, "
            f"got {action_optional_response.supports_response}"
        )

        assert action_only_response.supports_response == SupportsResponseType.ONLY, (
            f"action_only_response should have supports_response=ONLY, "
            f"got {action_only_response.supports_response}"
        )

        assert action_nested_json.supports_response == SupportsResponseType.ONLY, (
            f"action_nested_json should have supports_response=ONLY, "
            f"got {action_nested_json.supports_response}"
        )

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
        # No response expected for this action
        response = await client.execute_service(
            action_no_response, {"message": "test_message"}
        )
        assert response is None, "action_no_response should not return a response"
        await asyncio.wait_for(no_response_future, timeout=5.0)

        # Test action_status_response with success (supports_response: status)
        response = await client.execute_service(
            action_status_response,
            {"should_succeed": True},
            return_response=True,
        )
        await asyncio.wait_for(status_success_future, timeout=5.0)
        assert response is not None, "Expected response for status action"
        assert response.success is True, (
            f"Expected success=True, got {response.success}"
        )
        assert response.error_message == "", (
            f"Expected empty error_message, got '{response.error_message}'"
        )

        # Test action_status_response with error
        response = await client.execute_service(
            action_status_response,
            {"should_succeed": False},
            return_response=True,
        )
        await asyncio.wait_for(status_error_future, timeout=5.0)
        assert response is not None, "Expected response for status action"
        assert response.success is False, (
            f"Expected success=False, got {response.success}"
        )
        assert "Intentional failure" in response.error_message, (
            f"Expected error message containing 'Intentional failure', "
            f"got '{response.error_message}'"
        )

        # Test action_optional_response (supports_response: optional)
        response = await client.execute_service(
            action_optional_response,
            {"value": 42},
            return_response=True,
        )
        await asyncio.wait_for(optional_response_future, timeout=5.0)
        assert response is not None, "Expected response for optional action"
        assert response.success is True, (
            f"Expected success=True, got {response.success}"
        )
        # Parse response data as JSON
        response_json = json.loads(response.response_data.decode("utf-8"))
        assert response_json["input"] == 42, (
            f"Expected input=42, got {response_json.get('input')}"
        )
        assert response_json["doubled"] == 84, (
            f"Expected doubled=84, got {response_json.get('doubled')}"
        )

        # Test action_only_response (supports_response: only)
        response = await client.execute_service(
            action_only_response,
            {"name": "World"},
            return_response=True,
        )
        await asyncio.wait_for(only_response_future, timeout=5.0)
        assert response is not None, "Expected response for only action"
        assert response.success is True, (
            f"Expected success=True, got {response.success}"
        )
        response_json = json.loads(response.response_data.decode("utf-8"))
        assert response_json["greeting"] == "Hello, World!", (
            f"Expected greeting='Hello, World!', got {response_json.get('greeting')}"
        )
        assert response_json["length"] == 5, (
            f"Expected length=5, got {response_json.get('length')}"
        )

        # Test action_nested_json
        response = await client.execute_service(
            action_nested_json,
            {},
            return_response=True,
        )
        await asyncio.wait_for(nested_json_future, timeout=5.0)
        assert response is not None, "Expected response for nested json action"
        assert response.success is True, (
            f"Expected success=True, got {response.success}"
        )
        response_json = json.loads(response.response_data.decode("utf-8"))
        # Verify nested structure
        assert response_json["config"]["wifi"]["connected"] is True
        assert response_json["config"]["api"]["port"] == 6053
        assert response_json["items"][0] == "first"
        assert response_json["items"][1] == "second"
