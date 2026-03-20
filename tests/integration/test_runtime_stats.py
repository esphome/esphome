"""Test runtime statistics component."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_runtime_stats(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test runtime stats logs statistics at configured interval and tracks components."""
    loop = asyncio.get_running_loop()

    # Track how many times we see the total stats
    stats_count = 0
    first_stats_future = loop.create_future()
    second_stats_future = loop.create_future()

    # Track component stats
    component_stats_found = set()

    # Patterns to match - need to handle ANSI color codes and timestamps
    # The log format is: [HH:MM:SS][color codes][I][tag]: message
    total_stats_pattern = re.compile(r"Total stats \(since boot\):")
    # Match component names that may include dots (e.g., template.sensor)
    component_pattern = re.compile(
        r"^\[[^\]]+\].*?\s+([\w.]+):\s+count=(\d+),\s+avg=([\d.]+)ms"
    )

    def check_output(line: str) -> None:
        """Check log output for runtime stats messages."""
        nonlocal stats_count

        # Check for total stats line
        if total_stats_pattern.search(line):
            stats_count += 1

            if stats_count == 1 and not first_stats_future.done():
                first_stats_future.set_result(True)
            elif stats_count == 2 and not second_stats_future.done():
                second_stats_future.set_result(True)

        # Check for component stats
        match = component_pattern.match(line)
        if match:
            component_name = match.group(1)
            component_stats_found.add(component_name)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device is connected
        device_info = await client.device_info()
        assert device_info is not None

        # Wait for first "Total stats" log (should happen at 1s)
        try:
            await asyncio.wait_for(first_stats_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("First 'Total stats' log not seen within 5 seconds")

        # Wait for second "Total stats" log (should happen at 2s)
        try:
            await asyncio.wait_for(second_stats_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(f"Second 'Total stats' log not seen. Total seen: {stats_count}")

        # Verify we got at least 2 stats logs
        assert stats_count >= 2, (
            f"Expected at least 2 'Total stats' logs, got {stats_count}"
        )

        # Verify we found stats for our components
        assert "template.sensor" in component_stats_found, (
            f"Expected template.sensor stats, found: {component_stats_found}"
        )
        assert "template.switch" in component_stats_found, (
            f"Expected template.switch stats, found: {component_stats_found}"
        )
