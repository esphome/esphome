"""Integration test for API conditional memory optimization with triggers and services."""

from __future__ import annotations

import asyncio
import collections
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
    expected = {0, 1, 2, 3, 4}

    # Patterns to match in logs
    after_wait_until_pattern = re.compile(r"AFTER wait_until ARG (\d+)")
    after_script_wait_pattern = re.compile(r"AFTER script\.wait ARG (\d+)")
    after_repeat_pattern = re.compile(r"AFTER repeat ARG (\d+)")
    in_repeat_pattern = re.compile(r"IN repeat (\d+) ARG (\d+)")
    after_while_pattern = re.compile(r"AFTER while ARG (\d+)")
    in_while_pattern = re.compile(r"IN while ARG (\d+)")

    after_wait_until_args = []
    after_script_wait_args = []
    after_while_args = []
    in_while_args = []
    after_repeat_args = []
    in_repeat_args = collections.defaultdict(list)

    def check_output(line: str) -> None:
        """Check log output for expected messages."""
        if test_complete.is_set():
            return

        if mo := after_wait_until_pattern.search(line):
            after_wait_until_args.append(int(mo.group(1)))
        elif mo := after_script_wait_pattern.search(line):
            after_script_wait_args.append(int(mo.group(1)))
        elif mo := in_while_pattern.search(line):
            in_while_args.append(int(mo.group(1)))
        elif mo := after_while_pattern.search(line):
            after_while_args.append(int(mo.group(1)))
        elif mo := in_repeat_pattern.search(line):
            in_repeat_args[int(mo.group(1))].append(int(mo.group(2)))
        elif mo := after_repeat_pattern.search(line):
            after_repeat_args.append(int(mo.group(1)))
            if len(after_repeat_args) == len(expected):
                test_complete.set()

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        # Verify device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "action-concurrent-reentry"

        # Wait for tests to complete with timeout
        try:
            await asyncio.wait_for(test_complete.wait(), timeout=8.0)
        except TimeoutError:
            pytest.fail("test timed out")

        # order may change, but all args must be present
        for args in in_repeat_args.values():
            assert set(args) == expected
        assert set(in_repeat_args.keys()) == {0, 1, 2}
        assert set(after_wait_until_args) == expected, after_wait_until_args
        assert set(after_script_wait_args) == expected, after_script_wait_args
        assert set(after_repeat_args) == expected, after_repeat_args
        assert set(after_while_args) == expected, after_while_args
        assert dict(collections.Counter(in_while_args)) == {
            0: 5,
            1: 4,
            2: 3,
            3: 2,
            4: 1,
        }, in_while_args
