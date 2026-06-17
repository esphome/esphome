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
    main_loop_lines: list[dict[str, str]] = []

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

        # Check for main_loop overhead line
        ml_match = main_loop_pattern.search(line)
        if ml_match:
            main_loop_lines.append(ml_match.groupdict())

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
