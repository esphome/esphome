"""Regression test for blocking-warning log time attribution."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

WARN_PATTERN = re.compile(
    r"(\S+) took a long time for an operation \((\d+) ms\), max is (\d+) ms"
)
COMPLETE_PATTERN = re.compile(r"BLOCKING_WARNING_CASCADE_TEST_COMPLETE total=(\d+)")
PRIMARY_SOURCES = {"blocking_60", "blocking_90", "blocking_120"}


@pytest.mark.asyncio
async def test_blocking_warning_log_time_not_charged_to_next_operation(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Synchronous warning-log delays must not be charged to the next operation."""
    loop = asyncio.get_running_loop()
    complete = asyncio.Event()
    warnings: list[tuple[str, int, int]] = []
    injected_delay_total = 0

    def check_output(line: str) -> None:
        nonlocal injected_delay_total
        if match := WARN_PATTERN.search(line):
            warnings.append((match.group(1), int(match.group(2)), int(match.group(3))))
        if match := COMPLETE_PATTERN.search(line):
            injected_delay_total = int(match.group(1))
            loop.call_soon_threadsafe(complete.set)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        assert await client.device_info() is not None
        await asyncio.wait_for(complete.wait(), timeout=10.0)

    assert injected_delay_total == 270, (
        f"Expected 270 ms of injected warning-log delay, got {injected_delay_total} ms"
    )

    primary_warnings = [
        warning for warning in warnings if warning[0] in PRIMARY_SOURCES
    ]
    assert {warning[0] for warning in primary_warnings} == PRIMARY_SOURCES, (
        f"Expected one real blocking warning from each test script, got: {warnings}"
    )

    secondary_warnings = [
        warning for warning in warnings if warning[0] not in PRIMARY_SOURCES
    ]
    assert not secondary_warnings, (
        "Warning-handler time was incorrectly charged to the next operation: "
        f"{secondary_warnings}"
    )
