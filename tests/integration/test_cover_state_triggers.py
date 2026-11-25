"""Integration test for the generated cover on_opened, on_closed, on_opening, on_closing and on_idle callback."""

from __future__ import annotations

import re

from aioesphomeapi import ButtonInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

# Each step's publish and the triggers it must log, in order; on_closed logs after its delay.
STEPS = (
    ("closed_idle", ["idle", "closed delayed"]),
    ("opening", ["opening"]),
    ("open_idle", ["opened A", "opened B", "idle"]),
    ("tilt_only", []),
    ("closing", ["closing"]),
    ("closed_again", ["idle", "closed delayed"]),
)


@pytest.mark.asyncio
async def test_cover_state_triggers(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each trigger fires once on entering its state, in config order, including from its own publish."""
    waiter = LineWaiter()

    def fired() -> list[str]:
        return [
            m[1] for line in waiter.lines if (m := re.search(r"TRIG ([\w ]+)", line))
        ]

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        expected: list[str] = []
        for step, triggers in STEPS:
            button = require_entity(entities, f"step_{step}", ButtonInfo)
            client.button_command(button.key)
            await waiter.wait_for(f"STEP {step} done")
            expected += triggers
            await waiter.wait_count(
                "TRIG closed delayed", expected.count("closed delayed")
            )
            assert fired() == expected, step
