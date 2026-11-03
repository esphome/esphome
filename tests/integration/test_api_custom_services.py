"""Integration test for API custom services using CustomAPIDevice."""

from __future__ import annotations

from aioesphomeapi import UserServiceArgType
import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_custom_services(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test CustomAPIDevice services work correctly with custom_services: true."""
    waiter = LineWaiter()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "api-custom-services-test")

        # List services
        _, services = await client.list_entities_services()

        # Should have 9 services: 6 YAML + 3 CustomAPIDevice
        assert len(services) == 9, f"Expected 9 services, found {len(services)}"

        # Find our services
        yaml_service = require_service(services, "test_yaml_service")
        yaml_args_service = require_service(services, "test_yaml_service_with_args")
        yaml_many_args_service = require_service(
            services, "test_yaml_service_many_args"
        )
        custom_service = require_service(services, "custom_test_service")
        custom_args_service = require_service(services, "custom_service_with_args")
        custom_arrays_service = require_service(services, "custom_service_with_arrays")

        # Test YAML service
        await client.execute_service(yaml_service, {})
        await waiter.wait_for("YAML service called", timeout=5.0)

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
        await waiter.wait_for("YAML service with args: 123, test_value", timeout=5.0)

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
        await waiter.wait_for("YAML service many args: 42, 3.14, 1, hello", timeout=5.0)

        # YAML service with array arguments
        yaml_arrays_service = require_service(services, "test_yaml_service_arrays")
        assert [arg.type for arg in yaml_arrays_service.args] == [
            UserServiceArgType.BOOL_ARRAY,
            UserServiceArgType.INT_ARRAY,
            UserServiceArgType.FLOAT_ARRAY,
            UserServiceArgType.STRING_ARRAY,
        ]
        await client.execute_service(
            yaml_arrays_service,
            {
                "bool_arr": [True, False],
                "int_arr": [7, -8, 9],
                "float_arr": [2.5],
                "string_arr": ["ab", "cd"],
            },
        )
        await waiter.wait_for(
            "YAML service arrays: 1 0, 7 -8 9, 2.5, ab cd", timeout=5.0
        )

        # Arguments survive a delay
        await client.execute_service(
            require_service(services, "test_yaml_service_delay"),
            {"name": "late", "int_arr": [4, 5]},
        )
        await waiter.wait_for("YAML service delay before", timeout=5.0)
        await waiter.wait_for("YAML service delay after: late 2 5", timeout=5.0)

        # A lambda's change to an argument stays in that lambda
        await client.execute_service(
            require_service(services, "test_yaml_service_by_value"), {"x": 5}
        )
        await waiter.wait_for("YAML service by value first: 10", timeout=5.0)
        await waiter.wait_for("YAML service by value second: 15", timeout=5.0)
        await waiter.wait_for("YAML service by value last: 5", timeout=5.0)

        # Test simple CustomAPIDevice service
        await client.execute_service(custom_service, {})
        await waiter.wait_for("Custom test service called!", timeout=5.0)

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
        await waiter.wait_for(
            "Custom service called with: test_string, 456, 1, 78.90", timeout=5.0
        )

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
        await waiter.wait_for(
            "Array service called with 2 bools, 3 ints, 2 floats, 2 strings",
            timeout=5.0,
        )

        # Test Home Assistant state subscription (std::string API backward compatibility)
        # This verifies that custom_api_device.h can still use std::string overloads
        client.send_home_assistant_state("sensor.custom_test", "", "42.5")
        await waiter.wait_for(
            "This subscription uses std::string API for backward compatibility",
            timeout=5.0,
        )
