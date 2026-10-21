"""Integration test for script array parameters (issue #16367).

Verifies that script parameters of array types (`int[]`, `float[]`, `bool[]`,
`string[]`) compile and execute correctly. Prior to the fix in
`esphome/components/script/__init__.py`, the `script.execute` codegen emitted
the Python `repr` of the list (e.g. `return [42, 100];`) instead of a C++
braced initializer, causing compile failures.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

PATTERNS = {
    "ints": re.compile(r"ints size=(\d+) \[0\]=(-?\d+) \[1\]=(-?\d+)"),
    "floats": re.compile(r"floats size=(\d+) \[0\]=(-?\d+\.\d+) \[1\]=(-?\d+\.\d+)"),
    "bools": re.compile(r"bools size=(\d+) \[0\]=(\d+) \[1\]=(\d+)"),
    "strings": re.compile(r"strings size=(\d+) \[0\]=(\w+) \[1\]=(\w+)"),
}


@pytest.mark.asyncio
async def test_script_array_params(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Execute a script with int[], float[], bool[], string[] parameters."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        service = require_service(services, "run_array_script")
        await client.execute_service(service, {})

        values: dict[str, tuple[str, ...]] = {}
        for key, pattern in PATTERNS.items():
            line = await wait_or_fail(
                waiter.wait_for(f"{key} size="),
                5.0,
                "Did not receive all expected log lines",
                values=values,
            )
            assert (m := pattern.search(line)), line
            values[key] = m.groups()

        assert values == {
            "ints": ("2", "42", "100"),
            "floats": ("2", "1.50", "2.50"),
            "bools": ("2", "1", "0"),
            "strings": ("2", "hello", "world"),
        }
