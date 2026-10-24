"""Integration test for wait_until queue reentrancy.

When a wait_until completes, the rest of the action chain runs synchronously
from inside the wait queue processing. That chain can re-enter the very same
WaitUntilAction - for example a script with mode: restart that executes itself
as a retry pattern, or a waiter that stops its own script. Both used to mutate
the std::list while it was being iterated, corrupting it and crashing the
device (Guru Meditation StoreProhibited in _M_transfer).
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

ATTEMPT_RE = re.compile(r"attempt (\d+) done")
GATE_RE = re.compile(r"gate passed (\d+)")


@pytest.mark.asyncio
async def test_wait_until_reentrant_restart(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that re-entering a wait_until from its own continuation is safe."""
    waiter = LineWaiter()

    def attempts() -> list[int]:
        return [int(m[1]) for line in waiter.lines if (m := ATTEMPT_RE.search(line))]

    def gate_passed() -> list[int]:
        return [int(m[1]) for line in waiter.lines if (m := GATE_RE.search(line))]

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "wait-until-reentrant-restart")

        _, services = await client.list_entities_services()
        self_restart_service = require_service(services, "start_self_restart")
        stop_service = require_service(services, "start_stop_during_wait")

        # Scenario 1: the wait_until timeout continuation restarts its own
        # script five times, re-entering the same wait_until each time.
        await client.execute_service(self_restart_service, {})
        await wait_or_fail(
            waiter.wait_for("retry test complete", timeout=20.0),
            10.0,
            "Self-restart retry did not finish",
            attempts=attempts,
        )
        assert attempts() == [1, 2, 3, 4, 5], attempts()

        # Scenario 2: the first waiter through the gate stops the script while
        # the other waiters are still queued in the same wait_until; both the
        # not-yet-checked waiters (2, 3) and the already-checked still-waiting
        # blocker (0) must be cancelled, not fired.
        await client.execute_service(stop_service, {})
        await wait_or_fail(
            waiter.wait_for("stop test complete", timeout=20.0),
            10.0,
            "Stop-during-wait did not finish",
            gate_passed=gate_passed,
        )
        assert gate_passed() == [1], gate_passed()

        # If the cancelled blocker had been kept, its 1s wait_until timeout
        # would still fire - give it the chance and check it stays silent.
        await asyncio.sleep(1.5)
        assert gate_passed() == [1], gate_passed()
