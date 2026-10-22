"""Test sensor.in_range and number.in_range against a fixed truth table."""

from __future__ import annotations

import asyncio
import re

import pytest

from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction

# Bounds in the fixture: above 0, below 10, both inclusive. A NaN state is never in range.
# Columns: above only, below only, both bounds.
EXPECTED = {
    "neg": (0, 1, 0),  # -10
    "low": (1, 1, 1),  # 0, the lower bound
    "mid": (1, 1, 1),  # 5
    "high": (1, 1, 1),  # 10, the upper bound
    "over": (1, 0, 0),  # 20
    "nan": (0, 0, 0),
    "neginf": (0, 1, 0),
    "posinf": (1, 0, 0),
}
CONDITIONS = ("above", "below", "both")
RESULT_RE = re.compile(r"INRANGE (\w+) (sensor|number)_(above|below|both) ([01])")


@pytest.mark.asyncio
async def test_in_range_conditions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each in_range condition gives the same result for sensors and numbers."""
    loop = asyncio.get_running_loop()
    results: dict[tuple[str, str, str], int] = {}
    done = loop.create_future()

    def check_output(line: str) -> None:
        if match := RESULT_RE.search(line):
            label, entity, condition, value = match.groups()
            results[(label, entity, condition)] = int(value)
        elif "INRANGE done" in line and not done.done():
            done.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        service = require_service(services, "run_cases")
        await client.execute_service(service, {})
        await asyncio.wait_for(done, timeout=10.0)

    expected = {
        (label, entity, condition): row[index]
        for label, row in EXPECTED.items()
        for entity in ("sensor", "number")
        for index, condition in enumerate(CONDITIONS)
    }
    assert results == expected
