"""Integration test for FNV-1a hash functions."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

ANSI_PATTERN = re.compile(r"\x1b\[[0-9;]*m")
# Format: "[timestamp][level][FNV1A:line]: test_name PASSED"
RESULT_PATTERN = re.compile(r"\[FNV1A:\d+\]:\s+(\w+)\s+(PASSED|FAILED)")


@pytest.mark.asyncio
async def test_fnv1a_hash(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that FNV-1a hash functions work correctly."""

    waiter = LineWaiter()
    expected_tests = {"empty", "known_hello", "known_helloworld", "extend", "string"}

    def test_results() -> dict[str, str]:
        return {
            m.group(1): m.group(2)
            for line in waiter.lines
            if (m := RESULT_PATTERN.search(ANSI_PATTERN.sub("", line)))
        }

    async def wait_all_results() -> None:
        for name in expected_tests:
            await waiter.wait_for("[FNV1A:", f"]: {name} ")

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "fnv1a-hash-test")

        # Wait for all tests to complete or timeout
        await wait_or_fail(
            wait_all_results(), 2.0, "Tests timed out", test_results=test_results
        )
        results = test_results()

        # Verify all tests passed
        assert "empty" in results, "empty string test not found"
        assert results["empty"] == "PASSED", "empty string test failed"

        assert "known_hello" in results, "known_hello test not found"
        assert results["known_hello"] == "PASSED", "known_hello test failed"

        assert "known_helloworld" in results, "known_helloworld test not found"
        assert results["known_helloworld"] == "PASSED", "known_helloworld test failed"

        assert "extend" in results, "fnv1a_hash_extend test not found"
        assert results["extend"] == "PASSED", "fnv1a_hash_extend test failed"

        assert "string" in results, "std::string test not found"
        assert results["string"] == "PASSED", "std::string test failed"
