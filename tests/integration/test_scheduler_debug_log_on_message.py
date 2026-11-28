"""With debug_scheduler, on_message skips the scheduler's own lines, so an automation that runs a delay
neither deadlocks the scheduler at boot nor keeps triggering itself."""

from __future__ import annotations

import asyncio
import re
from typing import Any

from aioesphomeapi import LogLevel
import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction

SNAPSHOT = re.compile(r"(FIRST|SECOND)_SNAPSHOT delays_run=(\d+)")


@pytest.mark.asyncio
async def test_scheduler_debug_log_on_message(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The device boots, the automation's delays run, it stops triggering itself, and other listeners such as
    an API log subscriber still get the scheduler's lines."""
    waiter = LineWaiter()
    api_scheduler_line: asyncio.Future[str] = asyncio.get_running_loop().create_future()

    def on_log(msg: Any) -> None:
        text = msg.message.decode("utf-8", errors="replace")
        if "][scheduler" in text and not api_scheduler_line.done():
            api_scheduler_line.set_result(text)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "sched-debug-log")
        client.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
        first = await waiter.wait_for("FIRST_SNAPSHOT", timeout=10.0)
        second = await waiter.wait_for("SECOND_SNAPSHOT", timeout=10.0)
        # The subscriber still gets the scheduler's lines; only on_message skips them
        await asyncio.wait_for(api_scheduler_line, timeout=10.0)

    first_count = int(SNAPSHOT.search(first).group(2))
    second_count = int(SNAPSHOT.search(second).group(2))
    assert first_count > 0, "the on_message delay never ran"
    # 2 s apart: only the snapshot line itself and a few API lines may trigger a delay
    assert second_count - first_count <= 5, (
        f"on_message kept triggering itself: {first_count} -> {second_count} delays"
    )
