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

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_wait_until_reentrant_restart(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that re-entering a wait_until from its own continuation is safe."""
    retry_complete = asyncio.Event()
    stop_complete = asyncio.Event()

    attempt_pattern = re.compile(r"attempt (\d+) done")
    gate_pattern = re.compile(r"gate passed (\d+)")

    attempts: list[int] = []
    gate_passed: list[int] = []

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if mo := attempt_pattern.search(line):
            attempts.append(int(mo.group(1)))
        elif mo := gate_pattern.search(line):
            gate_passed.append(int(mo.group(1)))
        elif "retry test complete" in line:
            retry_complete.set()
        elif "stop test complete" in line:
            stop_complete.set()

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "wait-until-reentrant-restart"

        _, services = await client.list_entities_services()
        self_restart_service = next(
            (s for s in services if s.name == "start_self_restart"), None
        )
        assert self_restart_service is not None, "start_self_restart not found"
        stop_service = next(
            (s for s in services if s.name == "start_stop_during_wait"), None
        )
        assert stop_service is not None, "start_stop_during_wait not found"

        # Scenario 1: the wait_until timeout continuation restarts its own
        # script five times, re-entering the same wait_until each time.
        await client.execute_service(self_restart_service, {})
        try:
            await asyncio.wait_for(retry_complete.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail(f"Self-restart retry did not finish. Attempts: {attempts}")
        assert attempts == [1, 2, 3, 4, 5], attempts

        # Scenario 2: the first waiter through the gate stops the script while
        # the other waiters are still queued in the same wait_until; both the
        # not-yet-checked waiters (2, 3) and the already-checked still-waiting
        # blocker (0) must be cancelled, not fired.
        await client.execute_service(stop_service, {})
        try:
            await asyncio.wait_for(stop_complete.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail(f"Stop-during-wait did not finish. Gate passed: {gate_passed}")
        assert gate_passed == [1], gate_passed

        # If the cancelled blocker had been kept, its 1s wait_until timeout
        # would still fire - give it the chance and check it stays silent.
        await asyncio.sleep(1.5)
        assert gate_passed == [1], gate_passed
