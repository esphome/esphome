"""Test that NUMERIC_ID_INTERNAL and NUMERIC_ID cannot collide.

Verifies that InternalSchedulerID (used by core base classes like
PollingComponent and DelayAction) and uint32_t numeric IDs (used by
components) are in completely separate matching namespaces, even when
the underlying uint32_t values are identical and on the same component.
"""

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_scheduler_internal_id_no_collision(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that internal and numeric IDs with same value don't collide."""
    waiter = LineWaiter()

    def seen(text: str) -> bool:
        return any(text in line for line in waiter.lines)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "scheduler-internal-id-test")

        await wait_or_fail(
            waiter.wait_for("All collision tests complete"),
            5.0,
            "Not all collision tests completed within 5 seconds",
        )

        # Test 1: Both timeout types with same ID 0 must fire
        assert seen("Internal timeout 0 fired"), (
            "Internal timeout with ID 0 should have fired"
        )
        assert seen("Numeric timeout 0 fired"), (
            "Numeric timeout with ID 0 should have fired"
        )

        # Test 2: Cancelling numeric ID must NOT cancel internal ID
        assert seen("Internal timeout 1 survived cancel"), (
            "Internal timeout 1 should survive cancellation of numeric timeout 1"
        )
        assert not seen("ERROR: Numeric timeout 1 should have been cancelled"), (
            "Numeric timeout 1 should have been cancelled"
        )

        # Test 3: Cancelling internal ID must NOT cancel numeric ID
        assert seen("Numeric timeout 2 survived cancel"), (
            "Numeric timeout 2 should survive cancellation of internal timeout 2"
        )
        assert not seen("ERROR: Internal timeout 2 should have been cancelled"), (
            "Internal timeout 2 should have been cancelled"
        )

        # Test 4: Both interval types with same ID must fire independently
        assert seen("Internal interval 3 fired twice"), (
            "Internal interval 3 should have fired at least twice"
        )
        assert seen("Numeric interval 3 fired twice"), (
            "Numeric interval 3 should have fired at least twice"
        )

        # Test 5: String name and internal ID don't collide
        assert seen("String timeout collision_test fired"), (
            "String timeout 'collision_test' should have fired"
        )
        assert seen("Internal timeout 10 fired"), (
            "Internal timeout 10 should have fired alongside string timeout"
        )
