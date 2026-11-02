"""Test that only an interval with a startup delay waits before its first run."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .types import RunCompiledFunction

PLAIN_FIRST = re.compile(r"PLAIN_FIRST uptime=(\d+)")
DELAYED_FIRST = re.compile(
    r"DELAYED_FIRST uptime=(\d+) plain_count=(\d+) update_interval=(\d+)"
)


@pytest.mark.asyncio
async def test_interval_startup_delay(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """The plain interval runs from boot; the delayed one starts after 1500 ms."""
    waiter = LineWaiter()
    plain_future = waiter.expect(PLAIN_FIRST)
    delayed_future = waiter.expect(DELAYED_FIRST)

    async with run_compiled(yaml_config, line_callback=waiter.callback):
        plain = await waiter.wait(plain_future)
        delayed = await waiter.wait(delayed_future)

    # The plain interval first runs within one 100 ms period of boot.
    assert int(delayed.group(1)) - int(plain.group(1)) >= 1300
    # The plain interval kept running every 100 ms while the delayed one waited.
    assert int(delayed.group(2)) >= 10
    assert int(delayed.group(3)) == 100
