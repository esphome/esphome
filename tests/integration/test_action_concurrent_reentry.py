"""Integration test for API conditional memory optimization with triggers and services."""

from __future__ import annotations

import collections
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


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
    waiter = LineWaiter()
    expected = {0, 1, 2, 3, 4}

    def captures(pattern: str) -> list[tuple[int, ...]]:
        return [
            tuple(map(int, m.groups()))
            for line in waiter.lines
            if (m := re.search(pattern, line))
        ]

    def arg_list(pattern: str) -> list[int]:
        return [arg for (arg,) in captures(pattern)]

    # Run with log monitoring
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        # Verify device info
        await assert_device_name(client, "action-concurrent-reentry")

        # Wait for tests to complete with timeout
        await wait_or_fail(
            waiter.wait_for_each(*(f"AFTER repeat ARG {arg}" for arg in expected)),
            8.0,
            "test timed out",
        )
        after_wait_until_args = arg_list(r"AFTER wait_until ARG (\d+)")
        after_script_wait_args = arg_list(r"AFTER script\.wait ARG (\d+)")
        after_repeat_args = arg_list(r"AFTER repeat ARG (\d+)")
        after_while_args = arg_list(r"AFTER while ARG (\d+)")
        in_while_args = arg_list(r"IN while ARG (\d+)")
        in_repeat_args = collections.defaultdict(list)
        for iteration, arg in captures(r"IN repeat (\d+) ARG (\d+)"):
            in_repeat_args[iteration].append(arg)

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
