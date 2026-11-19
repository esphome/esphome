"""Regression test for blocking-warning log time attribution."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
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
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        assert await client.device_info() is not None
        complete_line = await waiter.wait_for(
            "BLOCKING_WARNING_CASCADE_TEST_COMPLETE total=", timeout=10.0
        )

    assert (match := COMPLETE_PATTERN.search(complete_line)), complete_line
    injected_delay_total = int(match.group(1))
    warnings = [
        (m.group(1), int(m.group(2)), int(m.group(3)))
        for line in waiter.lines
        if (m := WARN_PATTERN.search(line))
    ]

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
