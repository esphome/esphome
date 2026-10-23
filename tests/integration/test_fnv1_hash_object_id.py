"""Integration test for fnv1_hash_object_id function.

This test verifies that the C++ fnv1_hash_object_id() function in
esphome/core/helpers.h produces the same hash values as the Python
fnv1_hash_object_id() function in esphome/helpers.py.

If this test fails, one of the implementations has diverged and needs
to be updated to match the other.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

ANSI_PATTERN = re.compile(r"\x1b\[[0-9;]*m")
# Format: "[timestamp][level][FNV1_OID:line]: test_name PASSED"
RESULT_PATTERN = re.compile(r"\[FNV1_OID:\d+\]:\s+(\w+)\s+(PASSED|FAILED)")


@pytest.mark.asyncio
async def test_fnv1_hash_object_id(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that C++ fnv1_hash_object_id matches Python implementation."""

    waiter = LineWaiter()
    expected_tests = {
        "foo",
        "upper",
        "space",
        "underscore",
        "hyphen",
        "special",
        "complex",
        "empty",
    }

    def test_results() -> dict[str, str]:
        return {
            m.group(1): m.group(2)
            for line in waiter.lines
            if (m := RESULT_PATTERN.search(ANSI_PATTERN.sub("", line)))
        }

    async def wait_all_results() -> None:
        for name in expected_tests:
            await waiter.wait_for("[FNV1_OID:", f"]: {name} ")

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "fnv1-hash-object-id-test")

        # Wait for all tests to complete or timeout
        await wait_or_fail(
            wait_all_results(), 2.0, "Tests timed out", test_results=test_results
        )
        results = test_results()

        # Verify all tests passed
        for test_name in expected_tests:
            assert test_name in results, f"{test_name} test not found"
            assert results[test_name] == "PASSED", (
                f"{test_name} test failed - C++ and Python hash mismatch"
            )
