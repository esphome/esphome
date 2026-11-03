"""Test for recursive timeout scheduling - scheduling timeouts from within timeout callbacks."""

import asyncio

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_scheduler_recursive_timeout(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that scheduling timeouts from within timeout callbacks works correctly."""

    # Log line marking each step, in the expected order
    steps = {
        "Executing initial timeout": "initial_timeout",
        "Executing nested timeout 1": "nested_timeout_1",
        "Executing nested timeout 2": "nested_timeout_2",
        "Recursive timeout test complete": "test_complete",
    }
    expected_sequence = list(steps.values())

    waiter = LineWaiter()
    test_complete_future = waiter.expect("Recursive timeout test complete")

    def execution_sequence() -> list[str]:
        return [
            name
            for line in waiter.lines
            for text, name in steps.items()
            if text in line
        ]

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify we can connect
        await assert_device_name(client, "sched-recursive-timeout")

        # List entities and services
        _, services = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test service
        run_test_service = require_service(services, "run_recursive_timeout_test")

        # Call the service to start the test
        await client.execute_service(run_test_service, {})

        # Wait for test to complete
        await wait_or_fail(
            test_complete_future,
            10.0,
            "Recursive timeout test timed out",
            execution_sequence=execution_sequence,
        )
        sequence = execution_sequence()

        # Verify execution sequence
        assert sequence == expected_sequence, (
            f"Execution sequence mismatch. Expected {expected_sequence}, got {sequence}"
        )

        # Verify we got exactly 4 events (Initial + Level 1 + Level 2 + Complete)
        assert len(sequence) == 4, f"Expected 4 events but got {len(sequence)}"
