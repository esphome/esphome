"""Test that NUMERIC_ID_INTERNAL and NUMERIC_ID cannot collide.

Verifies that InternalSchedulerID (used by core base classes like
PollingComponent and DelayAction) and uint32_t numeric IDs (used by
components) are in completely separate matching namespaces, even when
the underlying uint32_t values are identical and on the same component.
"""

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_scheduler_internal_id_no_collision(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that internal and numeric IDs with same value don't collide."""
    # Test 1: Both types fire independently with same ID
    internal_timeout_0_fired = asyncio.Event()
    numeric_timeout_0_fired = asyncio.Event()

    # Test 2: Cancelling numeric doesn't cancel internal
    internal_timeout_1_survived = asyncio.Event()
    numeric_timeout_1_error = asyncio.Event()

    # Test 3: Cancelling internal doesn't cancel numeric
    numeric_timeout_2_survived = asyncio.Event()
    internal_timeout_2_error = asyncio.Event()

    # Test 4: Both interval types fire independently
    internal_interval_3_done = asyncio.Event()
    numeric_interval_3_done = asyncio.Event()

    # Test 5: String name doesn't collide with internal ID
    string_timeout_fired = asyncio.Event()
    internal_timeout_10_fired = asyncio.Event()

    # Completion
    all_tests_complete = asyncio.Event()

    def on_log_line(line: str) -> None:
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        if "Internal timeout 0 fired" in clean_line:
            internal_timeout_0_fired.set()
        elif "Numeric timeout 0 fired" in clean_line:
            numeric_timeout_0_fired.set()
        elif "Internal timeout 1 survived cancel" in clean_line:
            internal_timeout_1_survived.set()
        elif "ERROR: Numeric timeout 1 should have been cancelled" in clean_line:
            numeric_timeout_1_error.set()
        elif "Numeric timeout 2 survived cancel" in clean_line:
            numeric_timeout_2_survived.set()
        elif "ERROR: Internal timeout 2 should have been cancelled" in clean_line:
            internal_timeout_2_error.set()
        elif "Internal interval 3 fired twice" in clean_line:
            internal_interval_3_done.set()
        elif "Numeric interval 3 fired twice" in clean_line:
            numeric_interval_3_done.set()
        elif "String timeout collision_test fired" in clean_line:
            string_timeout_fired.set()
        elif "Internal timeout 10 fired" in clean_line:
            internal_timeout_10_fired.set()
        elif "All collision tests complete" in clean_line:
            all_tests_complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "scheduler-internal-id-test"

        try:
            await asyncio.wait_for(all_tests_complete.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail("Not all collision tests completed within 5 seconds")

        # Test 1: Both timeout types with same ID 0 must fire
        assert internal_timeout_0_fired.is_set(), (
            "Internal timeout with ID 0 should have fired"
        )
        assert numeric_timeout_0_fired.is_set(), (
            "Numeric timeout with ID 0 should have fired"
        )

        # Test 2: Cancelling numeric ID must NOT cancel internal ID
        assert internal_timeout_1_survived.is_set(), (
            "Internal timeout 1 should survive cancellation of numeric timeout 1"
        )
        assert not numeric_timeout_1_error.is_set(), (
            "Numeric timeout 1 should have been cancelled"
        )

        # Test 3: Cancelling internal ID must NOT cancel numeric ID
        assert numeric_timeout_2_survived.is_set(), (
            "Numeric timeout 2 should survive cancellation of internal timeout 2"
        )
        assert not internal_timeout_2_error.is_set(), (
            "Internal timeout 2 should have been cancelled"
        )

        # Test 4: Both interval types with same ID must fire independently
        assert internal_interval_3_done.is_set(), (
            "Internal interval 3 should have fired at least twice"
        )
        assert numeric_interval_3_done.is_set(), (
            "Numeric interval 3 should have fired at least twice"
        )

        # Test 5: String name and internal ID don't collide
        assert string_timeout_fired.is_set(), (
            "String timeout 'collision_test' should have fired"
        )
        assert internal_timeout_10_fired.is_set(), (
            "Internal timeout 10 should have fired alongside string timeout"
        )
