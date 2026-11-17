"""Test runtime statistics component."""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_runtime_stats(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test runtime stats logs statistics at configured interval and tracks components."""
    waiter = LineWaiter()

    # Patterns to match - need to handle ANSI color codes and timestamps
    # The log format is: [HH:MM:SS][color codes][I][tag]: message
    total_stats_pattern = re.compile(r"Total stats \(since boot\):")
    # Match component names that may include dots (e.g., template.sensor)
    component_pattern = re.compile(
        r"^\[[^\]]+\].*?\s+([\w.]+):\s+count=(\d+),\s+avg=([\d.]+)ms"
    )
    # Main loop overhead line emitted by runtime_stats
    main_loop_pattern = re.compile(
        r"main_loop:\s+iters=(?P<iters>\d+),\s+"
        r"active_avg=(?P<active_avg>[\d.]+)ms,\s+"
        r"active_max=(?P<active_max>[\d.]+)ms,\s+"
        r"active_total=(?P<active_total>[\d.]+)ms,\s+"
        r"overhead_total=(?P<overhead_total>[\d.]+)ms"
    )

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device is connected
        device_info = await client.device_info()
        assert device_info is not None

        # Wait for first "Total stats" log (should happen at 1s)
        await waiter.wait_count(
            total_stats_pattern,
            1,
            5.0,
            "First 'Total stats' log not seen within 5 seconds",
        )

        # Wait for second "Total stats" log (should happen at 2s)
        await waiter.wait_count(
            total_stats_pattern, 2, 5.0, "Second 'Total stats' log not seen"
        )

        # Verify we got at least 2 stats logs
        stats_count = waiter.count(total_stats_pattern)
        assert stats_count >= 2, (
            f"Expected at least 2 'Total stats' logs, got {stats_count}"
        )

        # Verify we found stats for our components
        component_stats_found = {
            match.group(1)
            for line in waiter.lines
            if (match := component_pattern.match(line))
        }
        assert "template.sensor" in component_stats_found, (
            f"Expected template.sensor stats, found: {component_stats_found}"
        )
        assert "template.switch" in component_stats_found, (
            f"Expected template.switch stats, found: {component_stats_found}"
        )

        main_loop_lines = [
            match.groupdict()
            for line in waiter.lines
            if (match := main_loop_pattern.search(line))
        ]
        # Verify the main_loop overhead line is emitted (at least once for
        # the period section and once for the total section, per log cycle).
        assert len(main_loop_lines) >= 2, (
            f"Expected at least 2 main_loop lines, got {len(main_loop_lines)}"
        )
        for fields in main_loop_lines:
            assert int(fields["iters"]) > 0, f"iters should be > 0: {fields}"
            assert float(fields["active_total"]) > 0.0, (
                f"active_total should be > 0: {fields}"
            )
            assert float(fields["active_avg"]) >= 0.0, (
                f"active_avg should be >= 0: {fields}"
            )
            # overhead_total is derived and may be 0 if components dominate,
            # but the field must still be present and parseable as a float.
            assert float(fields["overhead_total"]) >= 0.0, (
                f"overhead_total should be >= 0: {fields}"
            )
