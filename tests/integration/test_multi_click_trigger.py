"""Integration test for on_multi_click binary sensor automation.

Tests that on_multi_click correctly triggers for single click, double click,
and long press patterns using a template binary sensor with timing
orchestrated entirely in YAML.

"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction

COUNT_PATTERN = re.compile(r"count=(\d+)")


@pytest.mark.asyncio
async def test_multi_click_trigger(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that on_multi_click triggers for single, double, and long press patterns."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _entities, services = await client.list_entities_services()

        test_service = require_service(services, "run_all_tests")

        # Kick off the entire test sequence (runs in YAML with delays)
        await client.execute_service(test_service, {})

        for label in ("SINGLE_CLICK", "DOUBLE_CLICK", "LONG_PRESS"):
            line = await waiter.wait_for(f"{label} count=", timeout=5.0)
            match = COUNT_PATTERN.search(line)
            assert match is not None and match.group(1) == "1", (
                f"Expected {label} count=1, got: {line}"
            )
