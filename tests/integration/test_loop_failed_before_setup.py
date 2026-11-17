"""Integration test for a component that is marked failed before setup."""

from __future__ import annotations

import asyncio
import re

import pytest

from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_loop_failed_before_setup(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A component marked failed before setup must not run setup() or loop()."""
    failed_lines: list[str] = []
    healthy_looping = asyncio.Event()
    failed_reported = asyncio.Event()

    def on_log_line(line: str) -> None:
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)
        if "[failed_early]" in clean_line:
            failed_lines.append(clean_line)
        elif "[healthy] Loop count: 20" in clean_line:
            healthy_looping.set()
        if "is marked FAILED" in clean_line:
            failed_reported.set()

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "loop-failed-test")
        await wait_or_fail(healthy_looping, 10.0, "healthy component did not loop")
        assert failed_lines == [], "failed component ran setup() or loop()"
        await wait_or_fail(
            failed_reported, 10.0, "dump_config did not report the failed component"
        )
