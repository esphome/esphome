"""Tests for the integration test wait helpers."""

from __future__ import annotations

import asyncio

import pytest

from tests.integration.wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_wait_or_fail_returns_result() -> None:
    future: asyncio.Future[int] = asyncio.get_running_loop().create_future()
    future.set_result(42)
    assert await wait_or_fail(future) == 42


@pytest.mark.asyncio
async def test_wait_or_fail_accepts_coroutine_and_event() -> None:
    async def produce() -> str:
        return "done"

    assert await wait_or_fail(produce()) == "done"
    event = asyncio.Event()
    asyncio.get_running_loop().call_soon(event.set)
    assert await wait_or_fail(event) is True


@pytest.mark.asyncio
async def test_wait_or_fail_timeout_cancels_and_reports_context() -> None:
    future: asyncio.Future[None] = asyncio.get_running_loop().create_future()
    lines = [f"line {i}" for i in range(25)]
    seen: dict[str, int] = {}
    waiting = asyncio.ensure_future(
        wait_or_fail(future, 0.01, "no reply", lines=lines, seen=seen)
    )
    # Context is formatted on failure, so later changes to live objects show up.
    lines.append("late line")
    seen["a"] = 1
    with pytest.raises(pytest.fail.Exception) as err:
        await waiting
    message = str(err.value)
    assert message.startswith("no reply (after 0.01s)")
    assert "lines (last 20 of 26):" in message
    assert "line 5\n" not in message
    assert "line 6\n" in message
    assert message.endswith("late line\nseen: {'a': 1}")
    assert future.cancelled()
