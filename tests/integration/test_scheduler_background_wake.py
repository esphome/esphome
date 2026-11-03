"""A background scheduler insertion must interrupt the loop's current sleep.

Covers both set_timeout and defer: a zero-delay insert from another thread
takes the separate defer queue on multi-threaded builds.
"""

from __future__ import annotations

import re

from aioesphomeapi import ButtonInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

RESULT_RE = re.compile(
    r"SCHEDULER_WAKE_RESULT delay=\d+ elapsed=(\d+) loop_delta=(-?\d+)"
)


@pytest.mark.asyncio
async def test_scheduler_background_wake(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "scheduler-background-wake")
        entities, _ = await client.list_entities_services()
        for object_id, delay in (
            ("start_scheduler_timeout", 100),
            ("start_scheduler_defer", 0),
        ):
            start_button = require_entity(
                entities,
                object_id,
                ButtonInfo,
                description=f"{object_id} button",
            )
            client.button_command(start_button.key)

            line = await wait_or_fail(
                waiter.wait_for(
                    f"SCHEDULER_WAKE_RESULT delay={delay} elapsed=", timeout=20.0
                ),
                10.0,
                f"background insert with delay={delay} did not fire",
            )
            match = RESULT_RE.search(line)
            assert match, line
            elapsed, loop_delta = int(match[1]), int(match[2])

            assert elapsed < 1000, (
                f"background insert with delay={delay} should interrupt the "
                f"five-second sleep; it fired after {elapsed}ms"
            )
            assert loop_delta == 0, (
                f"scheduler-only wake must not run component loops; observed "
                f"loop_delta={loop_delta} for delay={delay}"
            )
