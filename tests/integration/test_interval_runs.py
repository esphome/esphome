"""Integration test that intervals run repeatedly and honour their startup delay."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .types import RunCompiledFunction

INLINE_FIRST = re.compile(r"INLINE first uptime=(\d+)")
STARTUP_FIRST = re.compile(r"STARTUP first uptime=(\d+) inline_count=(\d+)")
ACTION = re.compile(r"ACTION (start|end) (\d+)")


@pytest.mark.asyncio
async def test_interval_runs(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """Inline and delayed actions repeat; the startup delay holds the first run back."""
    waiter = LineWaiter()
    inline_future = waiter.expect(INLINE_FIRST)
    startup_future = waiter.expect(STARTUP_FIRST)

    async with run_compiled(yaml_config, line_callback=waiter.callback):
        inline = await waiter.wait(inline_future)
        startup = await waiter.wait(startup_future)
        await waiter.wait_for_each("INLINE ran 20 times", "ACTION end 3")

    # The startup delay held the first run back while the inline interval kept running
    assert int(startup.group(1)) - int(inline.group(1)) >= 800
    assert int(startup.group(2)) >= 10
    # Each run of the delayed actions finished before the next started
    actions = [m.groups() for line in waiter.lines if (m := ACTION.search(line))]
    assert actions[:6] == [
        ("start", "1"),
        ("end", "1"),
        ("start", "2"),
        ("end", "2"),
        ("start", "3"),
        ("end", "3"),
    ]
