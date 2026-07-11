"""Integration tests for blocking-warning source attribution.

A blocking operation that runs inside a deferred scheduler continuation (e.g. after a ``delay``
in a script) used to be reported as ``<null> took a long time for an operation (NN ms),
max is 30 ms`` because the continuation carries no component. The warning should instead name
the owning script and report the real threshold (50 ms).
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# Matches: "<source> took a long time for an operation (NN ms), max is NN ms"
WARN_PATTERN = re.compile(
    r"(\S+) took a long time for an operation \((\d+) ms\), max is (\d+) ms"
)


@pytest.mark.asyncio
async def test_scheduler_blocking_warning(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Deferred blocking work inside a script is attributed to the script, not "<null>"."""
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

        # on_boot runs the script, which defers via delay then busy-blocks > 50 ms in the
        # continuation, tripping the blocking warning.
        warning_line = await asyncio.wait_for(warning_future, timeout=10.0)

    # Must name the owning script, not "<null>" and not the generic fallback.
    assert "<null>" not in warning_line, (
        f"Warning should name the script, got: {warning_line}"
    )
    assert "a scheduled task" not in warning_line, (
        f"Warning should name the script, got: {warning_line}"
    )
    match = WARN_PATTERN.search(warning_line)
    assert match is not None
    assert match.group(1) == "blocking_script", (
        f"Warning should name 'blocking_script', got: {warning_line}"
    )
    # The reported threshold must be the real default (50 ms), not the stale "30 ms".
    assert match.group(3) == "50", f"Expected 'max is 50 ms', got: {warning_line}"


@pytest.mark.asyncio
async def test_scheduler_blocking_warning_generic_source(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A delay in a plain (non-script) automation logs the generic label, not a script name."""
    loop = asyncio.get_running_loop()
    warning_future: asyncio.Future[str] = loop.create_future()

    def check_output(line: str) -> None:
        if WARN_PATTERN.search(line) and not warning_future.done():
            warning_future.set_result(line)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        assert await client.device_info() is not None
        warning_line = await asyncio.wait_for(warning_future, timeout=10.0)

    assert "a scheduled task took a long time" in warning_line, (
        f"Non-script deferred work should log the generic label, got: {warning_line}"
    )
    assert "<null>" not in warning_line
    match = WARN_PATTERN.search(warning_line)
    assert match is not None and match.group(3) == "50", (
        f"Expected 'max is 50 ms', got: {warning_line}"
    )


@pytest.mark.asyncio
async def test_scheduler_delay_runs_on_failed_component(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A delay must still fire even when its context component is marked failed.

    Deferred (SELF_POINTER) scheduler items have no owning component, so the scheduler's
    failed-component skip must not drop them.
    """
    loop = asyncio.get_running_loop()
    fired: asyncio.Future[bool] = loop.create_future()

    def check_output(line: str) -> None:
        if "DELAY_FIRED_AFTER_FAIL" in line and not fired.done():
            fired.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        assert await client.device_info() is not None
        # If the failed host component wrongly dropped the delay, this times out.
        await asyncio.wait_for(fired, timeout=10.0)
