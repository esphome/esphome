"""Integration test for script array parameters (issue #16367).

Verifies that script parameters of array types (`int[]`, `float[]`, `bool[]`,
`string[]`) compile and execute correctly. Prior to the fix in
`esphome/components/script/__init__.py`, the `script.execute` codegen emitted
the Python `repr` of the list (e.g. `return [42, 100];`) instead of a C++
braced initializer, causing compile failures.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_script_array_params(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Execute a script with int[], float[], bool[], string[] parameters."""
    loop = asyncio.get_running_loop()
    seen: dict[str, str] = {}
    done = loop.create_future()

    patterns = {
        "ints": re.compile(r"ints size=(\d+) \[0\]=(-?\d+) \[1\]=(-?\d+)"),
        "floats": re.compile(
            r"floats size=(\d+) \[0\]=(-?\d+\.\d+) \[1\]=(-?\d+\.\d+)"
        ),
        "bools": re.compile(r"bools size=(\d+) \[0\]=(\d+) \[1\]=(\d+)"),
        "strings": re.compile(r"strings size=(\d+) \[0\]=(\w+) \[1\]=(\w+)"),
    }

    def check_output(line: str) -> None:
        for key, pat in patterns.items():
            if (m := pat.search(line)) and key not in seen:
                seen[key] = m.group(0)
        if len(seen) == len(patterns) and not done.done():
            done.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        service = next((s for s in services if s.name == "run_array_script"), None)
        assert service is not None, "run_array_script service not found"
        await client.execute_service(service, {})

        try:
            await asyncio.wait_for(done, timeout=5.0)
        except TimeoutError:
            pytest.fail(f"Did not receive all expected log lines. Saw: {seen}")

        assert (m := patterns["ints"].search(seen["ints"]))
        assert m.group(1) == "2" and m.group(2) == "42" and m.group(3) == "100"

        assert (m := patterns["floats"].search(seen["floats"]))
        assert m.group(1) == "2" and m.group(2) == "1.50" and m.group(3) == "2.50"

        assert (m := patterns["bools"].search(seen["bools"]))
        assert m.group(1) == "2" and m.group(2) == "1" and m.group(3) == "0"

        assert (m := patterns["strings"].search(seen["strings"]))
        assert m.group(1) == "2" and m.group(2) == "hello" and m.group(3) == "world"
