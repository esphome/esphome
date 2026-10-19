"""Tests for the integration test LineWaiter."""

from __future__ import annotations

import asyncio
import re

import pytest

from tests.integration.log_utils import LineWaiter


@pytest.mark.asyncio
async def test_expect_matches_only_later_lines() -> None:
    waiter = LineWaiter()
    waiter.callback("[I][app]: ready")
    ready = waiter.expect("ready")
    value = waiter.expect(re.compile(r"value=(\d+)"))
    assert not ready.done()

    waiter.callback("[D][x]: value=42")
    waiter.callback("[I][app]: ready again")
    assert await waiter.wait(ready) == "[I][app]: ready again"
    match = await waiter.wait(value)
    assert isinstance(match, re.Match)
    assert match.group(1) == "42"
    # wait_for still matches lines seen before the call.
    assert await waiter.wait_for("app", "ready") == "[I][app]: ready"


@pytest.mark.asyncio
async def test_wait_timeout_reports_pattern_and_lines() -> None:
    waiter = LineWaiter()
    for i in range(30):
        waiter.callback(f"line {i}")
    future = waiter.expect("never")
    with pytest.raises(pytest.fail.Exception) as err:
        await waiter.wait(future, 0.01, "boot message missing")
    message = str(err.value)
    assert message.startswith("boot message missing (after 0.01s)")
    assert "pattern: 'never'" in message
    assert "lines (last 20 of 30):" in message
    assert message.endswith("line 29")
    assert future.cancelled()
    # The cancelled expectation is dropped on the next line.
    waiter.callback("never")
    assert not waiter._expectations


def test_count() -> None:
    waiter = LineWaiter()
    for line in ("tick 1", "tock", "tick 2"):
        waiter.callback(line)
    assert waiter.count("tick") == 2
    assert waiter.count(re.compile(r"t\w+k$")) == 1
    assert waiter.count("missing") == 0


@pytest.mark.asyncio
async def test_expect_resolves_from_callback_while_waiting() -> None:
    waiter = LineWaiter()
    first = waiter.expect("first")
    second = waiter.expect("second")
    loop = asyncio.get_running_loop()
    loop.call_soon(waiter.callback, "second")
    loop.call_soon(waiter.callback, "first")
    assert await waiter.wait(first) == "first"
    assert await waiter.wait(second) == "second"
