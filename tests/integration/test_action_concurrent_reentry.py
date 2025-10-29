"""Integration test for API conditional memory optimization with triggers and services."""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.xfail(reason="https://github.com/esphome/issues/issues/6534")
@pytest.mark.asyncio
async def test_action_concurrent_reentry(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """
    This test runs a script in parallel with varying arguments and verifies if
    each script keeps its original argument throughout its execution
    """
    test_complete = asyncio.Event()

    # Patterns to match in logs
    before_wait_pattern = re.compile(r"BEFORE WAIT ARG (\d+)")
    after_wait_pattern = re.compile(r"AFTER WAIT ARG (\d+)")

    before_wait_args = []
    after_wait_args = []

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if test_complete.is_set():
            return

        if mo := before_wait_pattern.search(line):
            before_wait_args.append(int(mo.group(1)))
        elif mo := after_wait_pattern.search(line):
            after_wait_args.append(int(mo.group(1)))
            if len(after_wait_args) == 3:
                test_complete.set()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "action-concurrent-reentry-test"

        # Wait for tests to complete with timeout
        try:
            await asyncio.wait_for(test_complete.wait(), timeout=8.0)
        except TimeoutError:
            pytest.fail("test timed out")

        assert before_wait_args == list(range(len(before_wait_args)))
        # order may change, but all args must be present
        assert set(before_wait_args) == set(after_wait_args)
