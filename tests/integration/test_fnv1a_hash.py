"""Integration test for FNV-1a hash functions."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_fnv1a_hash(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that FNV-1a hash functions work correctly."""

    test_results = {}
    all_tests_complete = asyncio.Event()
    expected_tests = {"empty", "known_hello", "known_helloworld", "extend", "string"}

    def on_log_line(line: str) -> None:
        """Capture log lines with test results."""
        # Strip ANSI escape codes
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)
        # Look for our test result messages
        # Format: "[timestamp][level][FNV1A:line]: test_name PASSED"
        match = re.search(r"\[FNV1A:\d+\]:\s+(\w+)\s+(PASSED|FAILED)", clean_line)
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
        assert device_info.name == "fnv1a-hash-test"

        # Wait for all tests to complete or timeout
        try:
            await asyncio.wait_for(all_tests_complete.wait(), timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Tests timed out. Got results for: {set(test_results.keys())}")

        # Verify all tests passed
        assert "empty" in test_results, "empty string test not found"
        assert test_results["empty"] == "PASSED", "empty string test failed"

        assert "known_hello" in test_results, "known_hello test not found"
        assert test_results["known_hello"] == "PASSED", "known_hello test failed"

        assert "known_helloworld" in test_results, "known_helloworld test not found"
        assert test_results["known_helloworld"] == "PASSED", (
            "known_helloworld test failed"
        )

        assert "extend" in test_results, "fnv1a_hash_extend test not found"
        assert test_results["extend"] == "PASSED", "fnv1a_hash_extend test failed"

        assert "string" in test_results, "std::string test not found"
        assert test_results["string"] == "PASSED", "std::string test failed"
