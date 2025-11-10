"""Integration test for PR #11676 mid-loop timing bug.

This test validates that wait_until timeouts work correctly when triggered
mid-component-loop, where App.get_loop_component_start_time() is stale.

The bug: When wait_until is triggered partway through a component's loop execution
(e.g., from a script or automation), the cached loop_component_start_time_ is stale
relative to when the action was actually triggered. This causes timeout calculations
to underflow and timeout immediately instead of waiting the specified duration.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_wait_until_mid_loop_timing(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that wait_until timeout works when triggered mid-component-loop.

    This test:
    1. Executes a script that delays 100ms (simulating component work)
    2. Then starts wait_until with 200ms timeout
    3. Verifies timeout takes ~200ms, not <50ms (immediate timeout bug)
    """
    loop = asyncio.get_running_loop()

    # Track test results
    test_results = {
        "timeout_duration": None,
        "passed": False,
        "failed": False,
        "bug_detected": False,
    }

    # Patterns for log messages
    timeout_duration = re.compile(r"wait_until completed after (\d+) ms")
    test_pass = re.compile(r"✓ Timeout duration correct")
    test_fail = re.compile(r"✗ Timeout duration WRONG")
    bug_pattern = re.compile(r"Likely BUG: Immediate timeout")
    test_passed = re.compile(r"✓ Test PASSED")
    test_failed = re.compile(r"✗ Test FAILED")

    test_complete = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for test results."""
        # Extract timeout duration
        match = timeout_duration.search(line)
        if match:
            test_results["timeout_duration"] = int(match.group(1))

        if test_pass.search(line):
            test_results["passed"] = True
        if test_fail.search(line):
            test_results["failed"] = True
        if bug_pattern.search(line):
            test_results["bug_detected"] = True

        # Final test result
        if (
            test_passed.search(line)
            or test_failed.search(line)
            and not test_complete.done()
        ):
            test_complete.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Get the test service
        _, services = await client.list_entities_services()
        test_service = next(
            (s for s in services if s.name == "test_mid_loop_timeout"), None
        )
        assert test_service is not None, "test_mid_loop_timeout service not found"

        # Execute the test
        client.execute_service(test_service, {})

        # Wait for test to complete (100ms delay + 200ms timeout + margins = ~500ms)
        await asyncio.wait_for(test_complete, timeout=5.0)

        # Verify results
        assert test_results["timeout_duration"] is not None, (
            "Timeout duration not reported"
        )
        assert test_results["passed"], (
            f"Test failed: wait_until took {test_results['timeout_duration']}ms, expected ~200ms. "
            f"Bug detected: {test_results['bug_detected']}"
        )
        assert not test_results["bug_detected"], (
            f"BUG DETECTED: wait_until timed out immediately ({test_results['timeout_duration']}ms) "
            "instead of waiting 200ms. This indicates stale loop_component_start_time."
        )

        # Additional validation: timeout should be ~200ms (150-250ms range)
        duration = test_results["timeout_duration"]
        assert 150 <= duration <= 250, (
            f"Timeout duration {duration}ms outside expected range (150-250ms). "
            f"This suggests timing regression from PR #11676."
        )
