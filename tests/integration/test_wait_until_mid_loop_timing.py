"""Integration test for PR #11676 mid-loop timing bug.

This test validates that wait_until timeouts work correctly when triggered
mid-component-loop, where App.get_loop_component_start_time() is stale.

The bug: When wait_until is triggered partway through a component's loop execution
(e.g., from a script or automation), the cached loop_component_start_time_ is stale
relative to when the action was actually triggered. This causes timeout calculations
to underflow and timeout immediately instead of waiting the specified duration.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction

DURATION = re.compile(r"wait_until completed after (\d+) ms")


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
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Get the test service
        _, services = await client.list_entities_services()
        test_service = require_service(services, "test_mid_loop_timeout")

        # Execute the test
        await client.execute_service(test_service, {})

        # Wait for the "Test PASSED" or "Test FAILED" verdict (100ms delay + 200ms timeout)
        await waiter.wait_for("Test ", ": wait_until", timeout=5.0)

        durations = [
            int(m.group(1)) for line in waiter.lines if (m := DURATION.search(line))
        ]
        duration = durations[-1] if durations else None
        passed = any("✓ Timeout duration correct" in line for line in waiter.lines)
        bug_detected = any(
            "Likely BUG: Immediate timeout" in line for line in waiter.lines
        )

        assert duration is not None, "Timeout duration not reported"
        assert passed, (
            f"Test failed: wait_until took {duration}ms, expected ~200ms. "
            f"Bug detected: {bug_detected}"
        )
        assert not bug_detected, (
            f"BUG DETECTED: wait_until timed out immediately ({duration}ms) "
            "instead of waiting 200ms. This indicates stale loop_component_start_time."
        )

        # Additional validation: timeout should be ~200ms (150-250ms range)
        assert 150 <= duration <= 250, (
            f"Timeout duration {duration}ms outside expected range (150-250ms). "
            f"This suggests timing regression from PR #11676."
        )
