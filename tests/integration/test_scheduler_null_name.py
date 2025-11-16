"""Test that scheduler handles NULL names safely without crashing."""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_null_name(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduler handles NULL names safely without crashing."""

    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[bool] = loop.create_future()

    # Pattern to match test completion
    test_complete_pattern = re.compile(r"Test completed successfully")

    def check_output(line: str) -> None:
        """Check log output for test completion."""
        if not test_complete_future.done() and test_complete_pattern.search(line):
            test_complete_future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-null-name"

        # List services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        test_null_name_service = next(
            (s for s in services if s.name == "test_null_name"), None
        )
        assert test_null_name_service is not None, "test_null_name service not found"

        # Execute the test
        client.execute_service(test_null_name_service, {})

        # Wait for test completion
        try:
            await asyncio.wait_for(test_complete_future, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                "Test did not complete within timeout - likely crashed due to NULL name"
            )
