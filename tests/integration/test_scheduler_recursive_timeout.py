"""Test for recursive timeout scheduling - scheduling timeouts from within timeout callbacks."""

import asyncio
from pathlib import Path

from aioesphomeapi import UserService
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_recursive_timeout(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduling timeouts from within timeout callbacks works correctly."""

    # Get the absolute path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Create a future to signal test completion
    loop = asyncio.get_running_loop()
    test_complete_future: asyncio.Future[None] = loop.create_future()

    # Track execution sequence
    execution_sequence: list[str] = []
    expected_sequence = [
        "initial_timeout",
        "nested_timeout_1",
        "nested_timeout_2",
        "test_complete",
    ]

    def on_log_line(line: str) -> None:
        # Track execution sequence
        if "Executing initial timeout" in line:
            execution_sequence.append("initial_timeout")
        elif "Executing nested timeout 1" in line:
            execution_sequence.append("nested_timeout_1")
        elif "Executing nested timeout 2" in line:
            execution_sequence.append("nested_timeout_2")
        elif "Recursive timeout test complete" in line:
            execution_sequence.append("test_complete")
            if not test_complete_future.done():
                test_complete_future.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "sched-recursive-timeout"

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        run_test_service: UserService | None = None
        for service in services:
            if service.name == "run_recursive_timeout_test":
                run_test_service = service
                break

        assert run_test_service is not None, (
            "run_recursive_timeout_test service not found"
        )

        # Call the service to start the test
        client.execute_service(run_test_service, {})

        # Wait for test to complete
        try:
            await asyncio.wait_for(test_complete_future, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"Recursive timeout test timed out. Got sequence: {execution_sequence}"
            )

        # Verify execution sequence
        assert execution_sequence == expected_sequence, (
            f"Execution sequence mismatch. Expected {expected_sequence}, "
            f"got {execution_sequence}"
        )

        # Verify we got exactly 4 events (Initial + Level 1 + Level 2 + Complete)
        assert len(execution_sequence) == 4, (
            f"Expected 4 events but got {len(execution_sequence)}"
        )
