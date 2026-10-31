"""Integration test that on_boot automations run in priority order and on_loop runs each loop."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

STEP = re.compile(r"BOOT step (-?[\d.]+(?: before delay)?|after delay)")


@pytest.mark.asyncio
async def test_core_automation_order(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """on_boot blocks run once, highest priority first; a delay finishes after setup."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected(),
    ):
        await waiter.wait_for_each("BOOT step after delay", "LOOP ran 3 times")
        steps = [
            match.group(1) for line in waiter.lines if (match := STEP.search(line))
        ]
        assert steps == [
            "700",
            "600",
            "250.5 before delay",
            "-100",
            "after delay",
        ]
        assert sum("BOOT runs 1" in line for line in waiter.lines) == 1
        assert not any("BOOT runs 2" in line for line in waiter.lines)
