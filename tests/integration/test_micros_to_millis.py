"""Integration test for micros_to_millis Euclidean decomposition."""

from __future__ import annotations

import asyncio
import re

import pytest

from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_micros_to_millis(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that micros_to_millis matches reference uint64 division."""

    all_passed = asyncio.Event()
    failures: list[str] = []

    def on_log_line(line: str) -> None:
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)
        if "ALL_PASSED" in clean_line:
            all_passed.set()
        elif "FAILED" in clean_line and "[MTM" in clean_line:
            failures.append(clean_line)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "micros-to-millis-test")

        await wait_or_fail(
            all_passed, 2.0, "micros_to_millis test timed out", failures=failures
        )

        assert not failures, f"micros_to_millis failures: {failures}"
