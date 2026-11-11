"""Integration test for Home Assistant action lambdas returning strings and numbers."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_api_string_lambda(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test Home Assistant action lambdas that return different types are sent as strings."""
    waiter = LineWaiter()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "api-string-lambda-test")

        # List services to find our test services
        _, services = await client.list_entities_services()

        # Find all test services
        string_service = require_service(services, "test_string_lambda")

        int_service = require_service(services, "test_int_lambda")

        float_service = require_service(services, "test_float_lambda")

        char_ptr_service = require_service(services, "test_char_ptr_lambda")

        # Execute all four services to test different lambda return types
        await client.execute_service(
            string_service, {"input_string": "STRING_FROM_LAMBDA"}
        )
        await client.execute_service(int_service, {"input_number": 42})
        await client.execute_service(float_service, {"input_float": 3.14})
        await client.execute_service(
            char_ptr_service, {"input_number": 123, "input_string": "test_string"}
        )

        # Confirms the lambdas compiled and executed
        await wait_or_fail(
            waiter.wait_for_each(
                "Service called with string: STRING_FROM_LAMBDA",
                "Service called with int: 42",
                "Service called with float: 3.14",
                "Service called with number for char* test: 123",
                timeout=10.0,
            ),
            5.0,
            "Not every lambda service logged its call",
            logged=waiter.lines,
        )
