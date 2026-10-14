"""Integration test for on_multi_click binary sensor automation.

Tests that on_multi_click correctly triggers for single click, double click,
and long press patterns using a template binary sensor with timing
orchestrated entirely in YAML.

"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_multi_click_trigger(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that on_multi_click triggers for single, double, and long press patterns."""
    loop = asyncio.get_running_loop()

    single_click_pattern = re.compile(r"SINGLE_CLICK count=(\d+)")
    double_click_pattern = re.compile(r"DOUBLE_CLICK count=(\d+)")
    long_press_pattern = re.compile(r"LONG_PRESS count=(\d+)")

    single_click_future: asyncio.Future[int] = loop.create_future()
    double_click_future: asyncio.Future[int] = loop.create_future()
    long_press_future: asyncio.Future[int] = loop.create_future()

    def check_output(line: str) -> None:
        """Check log output for multi-click trigger messages."""
        if m := single_click_pattern.search(line):
            if not single_click_future.done():
                single_click_future.set_result(int(m.group(1)))
        elif m := double_click_pattern.search(line):
            if not double_click_future.done():
                double_click_future.set_result(int(m.group(1)))
        elif (m := long_press_pattern.search(line)) and not long_press_future.done():
            long_press_future.set_result(int(m.group(1)))

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        _entities, services = await client.list_entities_services()

        test_service = next((s for s in services if s.name == "run_all_tests"), None)
        assert test_service is not None, "run_all_tests service not found"

        # Kick off the entire test sequence (runs in YAML with delays)
        await client.execute_service(test_service, {})

        # Wait for all three triggers
        try:
            count = await asyncio.wait_for(single_click_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for SINGLE_CLICK - on_multi_click did not trigger."
            )
        assert count == 1, f"Expected single click count=1, got {count}"

        try:
            count = await asyncio.wait_for(double_click_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for DOUBLE_CLICK - on_multi_click did not trigger."
            )
        assert count == 1, f"Expected double click count=1, got {count}"

        try:
            count = await asyncio.wait_for(long_press_future, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for LONG_PRESS - on_multi_click did not trigger."
            )
        assert count == 1, f"Expected long press count=1, got {count}"
