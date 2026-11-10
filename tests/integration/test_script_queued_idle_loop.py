"""Test that an idle queued script disables its loop and re-enables on demand."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_queued_idle_loop(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Assert the loop state transitions of a queued script via VV logs.

    Expected sequence: the idle script disables its loop on the first
    iteration after boot, re-enables it when an instance gets queued,
    and disables it again once the queue drains.
    """
    loop_state = re.compile(r"\bscript loop (disabled|enabled)\b")
    waiter = LineWaiter()

    def transitions() -> list[str]:
        return [m.group(1) for line in waiter.lines if (m := loop_state.search(line))]

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # The idle script must disable its loop on the first iteration
        await waiter.wait_count(loop_state, 1, 5.0, "loop never disabled at boot")
        assert transitions() == ["disabled"], f"Unexpected boot state: {transitions()}"

        _, services = await client.list_entities_services()
        run_twice = require_service(services, "run_twice")
        await client.execute_service(run_twice, {})

        # Queueing the second instance must re-enable the loop
        await waiter.wait_count(loop_state, 2, 2.0, "loop not re-enabled on queue")
        # Both runs must complete and the drained queue must disable it again
        await waiter.wait_count("idle_script: END", 2, 2.0, "both runs did not end")
        await waiter.wait_count(loop_state, 3, 2.0, "loop not disabled after drain")

        assert transitions() == ["disabled", "enabled", "disabled"], (
            f"Unexpected loop state sequence: {transitions()}"
        )
