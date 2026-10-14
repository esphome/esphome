"""Integration test for API custom services using CustomAPIDevice."""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

from aioesphomeapi import UserService, UserServiceArgType
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_custom_services(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test CustomAPIDevice services work correctly with custom_services: true."""
    # Get the path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track log messages
    yaml_service_future = loop.create_future()
    yaml_args_future = loop.create_future()
    yaml_many_args_future = loop.create_future()
    custom_service_future = loop.create_future()
    custom_args_future = loop.create_future()
    custom_arrays_future = loop.create_future()
    ha_state_future = loop.create_future()

    # Patterns to match in logs
    yaml_service_pattern = re.compile(r"YAML service called")
    yaml_args_pattern = re.compile(r"YAML service with args: 123, test_value")
    yaml_many_args_pattern = re.compile(r"YAML service many args: 42, 3\.14, 1, hello")
    custom_service_pattern = re.compile(r"Custom test service called!")
    custom_args_pattern = re.compile(
        r"Custom service called with: test_string, 456, 1, 78\.90"
    )
    custom_arrays_pattern = re.compile(
        r"Array service called with 2 bools, 3 ints, 2 floats, 2 strings"
    )
    ha_state_pattern = re.compile(
        r"This subscription uses std::string API for backward compatibility"
    )

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not yaml_service_future.done() and yaml_service_pattern.search(line):
            yaml_service_future.set_result(True)
        elif not yaml_args_future.done() and yaml_args_pattern.search(line):
            yaml_args_future.set_result(True)
        elif not yaml_many_args_future.done() and yaml_many_args_pattern.search(line):
            yaml_many_args_future.set_result(True)
        elif not custom_service_future.done() and custom_service_pattern.search(line):
            custom_service_future.set_result(True)
        elif not custom_args_future.done() and custom_args_pattern.search(line):
            custom_args_future.set_result(True)
        elif not custom_arrays_future.done() and custom_arrays_pattern.search(line):
            custom_arrays_future.set_result(True)
        elif not ha_state_future.done() and ha_state_pattern.search(line):
            ha_state_future.set_result(True)

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "api-custom-services-test"

        # List services
        _, services = await client.list_entities_services()

        # Should have 6 services: 3 YAML + 3 CustomAPIDevice
        assert len(services) == 6, f"Expected 6 services, found {len(services)}"

        # Find our services
        yaml_service: UserService | None = None
        yaml_args_service: UserService | None = None
        yaml_many_args_service: UserService | None = None
        custom_service: UserService | None = None
        custom_args_service: UserService | None = None
        custom_arrays_service: UserService | None = None

        for service in services:
            if service.name == "test_yaml_service":
                yaml_service = service
            elif service.name == "test_yaml_service_with_args":
                yaml_args_service = service
            elif service.name == "test_yaml_service_many_args":
                yaml_many_args_service = service
            elif service.name == "custom_test_service":
                custom_service = service
            elif service.name == "custom_service_with_args":
                custom_args_service = service
            elif service.name == "custom_service_with_arrays":
                custom_arrays_service = service

        assert yaml_service is not None, "test_yaml_service not found"
        assert yaml_args_service is not None, "test_yaml_service_with_args not found"
        assert yaml_many_args_service is not None, (
            "test_yaml_service_many_args not found"
        )
        assert custom_service is not None, "custom_test_service not found"
        assert custom_args_service is not None, "custom_service_with_args not found"
        assert custom_arrays_service is not None, "custom_service_with_arrays not found"

        # Test YAML service
        await client.execute_service(yaml_service, {})
        await asyncio.wait_for(yaml_service_future, timeout=5.0)

        # Verify YAML service with args arguments
        assert len(yaml_args_service.args) == 2
        yaml_args_types = {arg.name: arg.type for arg in yaml_args_service.args}
        assert yaml_args_types["my_int"] == UserServiceArgType.INT
        assert yaml_args_types["my_string"] == UserServiceArgType.STRING

        # Test YAML service with arguments
        await client.execute_service(
            yaml_args_service,
            {
                "my_int": 123,
                "my_string": "test_value",
            },
        )
        await asyncio.wait_for(yaml_args_future, timeout=5.0)

        # Verify YAML service with many args arguments
        assert len(yaml_many_args_service.args) == 4
        yaml_many_args_types = {
            arg.name: arg.type for arg in yaml_many_args_service.args
        }
        assert yaml_many_args_types["arg1"] == UserServiceArgType.INT
        assert yaml_many_args_types["arg2"] == UserServiceArgType.FLOAT
        assert yaml_many_args_types["arg3"] == UserServiceArgType.BOOL
        assert yaml_many_args_types["arg4"] == UserServiceArgType.STRING

        # Test YAML service with many arguments
        await client.execute_service(
            yaml_many_args_service,
            {
                "arg1": 42,
                "arg2": 3.14,
                "arg3": True,
                "arg4": "hello",
            },
        )
        await asyncio.wait_for(yaml_many_args_future, timeout=5.0)

        # Test simple CustomAPIDevice service
        await client.execute_service(custom_service, {})
        await asyncio.wait_for(custom_service_future, timeout=5.0)

        # Verify custom_args_service arguments
        assert len(custom_args_service.args) == 4
        arg_types = {arg.name: arg.type for arg in custom_args_service.args}
        assert arg_types["arg_string"] == UserServiceArgType.STRING
        assert arg_types["arg_int"] == UserServiceArgType.INT
        assert arg_types["arg_bool"] == UserServiceArgType.BOOL
        assert arg_types["arg_float"] == UserServiceArgType.FLOAT

        # Test CustomAPIDevice service with arguments
        await client.execute_service(
            custom_args_service,
            {
                "arg_string": "test_string",
                "arg_int": 456,
                "arg_bool": True,
                "arg_float": 78.9,
            },
        )
        await asyncio.wait_for(custom_args_future, timeout=5.0)

        # Verify array service arguments
        assert len(custom_arrays_service.args) == 4
        array_arg_types = {arg.name: arg.type for arg in custom_arrays_service.args}
        assert array_arg_types["bool_array"] == UserServiceArgType.BOOL_ARRAY
        assert array_arg_types["int_array"] == UserServiceArgType.INT_ARRAY
        assert array_arg_types["float_array"] == UserServiceArgType.FLOAT_ARRAY
        assert array_arg_types["string_array"] == UserServiceArgType.STRING_ARRAY

        # Test CustomAPIDevice service with arrays
        await client.execute_service(
            custom_arrays_service,
            {
                "bool_array": [True, False],
                "int_array": [1, 2, 3],
                "float_array": [1.1, 2.2],
                "string_array": ["hello", "world"],
            },
        )
        await asyncio.wait_for(custom_arrays_future, timeout=5.0)

        # Test Home Assistant state subscription (std::string API backward compatibility)
        # This verifies that custom_api_device.h can still use std::string overloads
        client.send_home_assistant_state("sensor.custom_test", "", "42.5")
        await asyncio.wait_for(ha_state_future, timeout=5.0)
