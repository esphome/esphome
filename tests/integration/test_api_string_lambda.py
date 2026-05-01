"""Integration test for TemplatableStringValue with string lambdas."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_string_lambda(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test TemplatableStringValue works with lambdas that return different types."""
    loop = asyncio.get_running_loop()

    # Track log messages for all four service calls
    string_called_future = loop.create_future()
    int_called_future = loop.create_future()
    float_called_future = loop.create_future()
    char_ptr_called_future = loop.create_future()

    # Patterns to match in logs - confirms the lambdas compiled and executed
    string_pattern = re.compile(r"Service called with string: STRING_FROM_LAMBDA")
    int_pattern = re.compile(r"Service called with int: 42")
    float_pattern = re.compile(r"Service called with float: 3\.14")
    char_ptr_pattern = re.compile(r"Service called with number for char\* test: 123")

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if not string_called_future.done() and string_pattern.search(line):
            string_called_future.set_result(True)
        if not int_called_future.done() and int_pattern.search(line):
            int_called_future.set_result(True)
        if not float_called_future.done() and float_pattern.search(line):
            float_called_future.set_result(True)
        if not char_ptr_called_future.done() and char_ptr_pattern.search(line):
            char_ptr_called_future.set_result(True)

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "api-string-lambda-test"

        # List services to find our test services
        _, services = await client.list_entities_services()

        # Find all test services
        string_service = next(
            (s for s in services if s.name == "test_string_lambda"), None
        )
        assert string_service is not None, "test_string_lambda service not found"

        int_service = next((s for s in services if s.name == "test_int_lambda"), None)
        assert int_service is not None, "test_int_lambda service not found"

        float_service = next(
            (s for s in services if s.name == "test_float_lambda"), None
        )
        assert float_service is not None, "test_float_lambda service not found"

        char_ptr_service = next(
            (s for s in services if s.name == "test_char_ptr_lambda"), None
        )
        assert char_ptr_service is not None, "test_char_ptr_lambda service not found"

        # Execute all four services to test different lambda return types
        await client.execute_service(
            string_service, {"input_string": "STRING_FROM_LAMBDA"}
        )
        await client.execute_service(int_service, {"input_number": 42})
        await client.execute_service(float_service, {"input_float": 3.14})
        await client.execute_service(
            char_ptr_service, {"input_number": 123, "input_string": "test_string"}
        )

        # Wait for all service log messages
        # This confirms the lambdas compiled successfully and executed
        try:
            await asyncio.wait_for(
                asyncio.gather(
                    string_called_future,
                    int_called_future,
                    float_called_future,
                    char_ptr_called_future,
                ),
                timeout=5.0,
            )
        except TimeoutError:
            pytest.fail(
                "One or more service log messages not received - lambda may have failed to compile or execute"
            )
