"""Test that a UDP send before the sockets are open is dropped with a log line.

The sockets are opened from the first loop() after the network connects, so a
udp.write from an early on_boot runs before they exist. It must be dropped with
a "Sockets not open yet" message rather than silently, and a later write must
not hit that path.
"""

from __future__ import annotations

import asyncio

import pytest

from .types import RunCompiledFunction


@pytest.mark.asyncio
async def test_udp_send_before_ready(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """An early send is dropped with a log line; a send after setup is not."""
    loop = asyncio.get_running_loop()
    write_done: asyncio.Future[None] = loop.create_future()
    events: list[str] = []

    def on_log_line(line: str) -> None:
        if "Sockets not open yet" in line:
            events.append("dropped")
        elif "Sockets ready" in line:
            events.append("ready")
        elif "WRITE_DONE" in line and not write_done.done():
            write_done.set_result(None)

    async with run_compiled(yaml_config, line_callback=on_log_line):
        try:
            await asyncio.wait_for(write_done, timeout=15.0)
        except TimeoutError:
            pytest.fail("WRITE_DONE never appeared")

    assert events == ["dropped", "ready"]
