"""Integration test for blocking-warning source attribution.

A blocking operation that runs inside a deferred scheduler continuation (e.g. after
a ``delay`` in a script/automation) used to be reported as
``<null> took a long time for an operation (NN ms), max is 30 ms`` because the
continuation carries no component. The warning should instead name the component that
was current when the delay was scheduled and report the real threshold (50 ms).
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# Matches: "<source> took a long time for an operation (NN ms), max is NN ms"
WARN_PATTERN = re.compile(
    r"took a long time for an operation \((\d+) ms\), max is (\d+) ms"
)


@pytest.mark.asyncio
async def test_scheduler_blocking_warning(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Deferred blocking work is attributed to a real component, not "<null>"."""
    loop = asyncio.get_running_loop()
    warning_future: asyncio.Future[str] = loop.create_future()

    def check_output(line: str) -> None:
        if WARN_PATTERN.search(line) and not warning_future.done():
            warning_future.set_result(line)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None

        # The interval fires, defers via delay, then busy-blocks > 50 ms in the
        # continuation, which should trip the blocking warning.
        warning_line = await asyncio.wait_for(warning_future, timeout=10.0)

    # The deferred block must be attributed to a real component, not "<null>".
    assert "<null>" not in warning_line, (
        f"Warning should name a component, got: {warning_line}"
    )
    # The delay was scheduled from a known component (the interval), so the warning
    # must name it rather than falling back to the generic scheduled-task label.
    assert "a scheduled task" not in warning_line, (
        f"Warning should name the interval component, got: {warning_line}"
    )
    # The reported threshold must be the real default (50 ms), not the stale "30 ms".
    match = WARN_PATTERN.search(warning_line)
    assert match is not None
    assert match.group(2) == "50", f"Expected 'max is 50 ms', got: {warning_line}"
