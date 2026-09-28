"""Integration test for fnv1_hash_object_id function.

This test verifies that the C++ fnv1_hash_object_id() function in
esphome/core/helpers.h produces the same hash values as the Python
fnv1_hash_object_id() function in esphome/helpers.py.

If this test fails, one of the implementations has diverged and needs
to be updated to match the other.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_fnv1_hash_object_id(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that C++ fnv1_hash_object_id matches Python implementation."""

    test_results: dict[str, str] = {}
    all_tests_complete = asyncio.Event()
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

    def on_log_line(line: str) -> None:
        """Capture log lines with test results."""
        # Strip ANSI escape codes
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)
        # Look for our test result messages
        # Format: "[timestamp][level][FNV1_OID:line]: test_name PASSED"
        match = re.search(r"\[FNV1_OID:\d+\]:\s+(\w+)\s+(PASSED|FAILED)", clean_line)
        if match:
            test_name = match.group(1)
            result = match.group(2)
            test_results[test_name] = result
            if set(test_results.keys()) >= expected_tests:
                all_tests_complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "fnv1-hash-object-id-test"

        # Wait for all tests to complete or timeout
        try:
            await asyncio.wait_for(all_tests_complete.wait(), timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Tests timed out. Got results for: {set(test_results.keys())}")

        # Verify all tests passed
        for test_name in expected_tests:
            assert test_name in test_results, f"{test_name} test not found"
            assert test_results[test_name] == "PASSED", (
                f"{test_name} test failed - C++ and Python hash mismatch"
            )
