"""Helpers for waiting on futures and events in integration tests."""

from __future__ import annotations

import asyncio
from collections import deque
from collections.abc import Awaitable, Mapping

import pytest

# How many trailing items of a collection are shown in a failure message.
TAIL_ITEMS = 20


def _format_context(context: dict[str, object]) -> str:
    """Render context values for a failure message, keeping only the tail of collections."""
    parts: list[str] = []
    for name, value in context.items():
        if isinstance(value, Mapping):
            items = [f"{key}: {item!r}" for key, item in value.items()]
        elif isinstance(value, (list, tuple, deque)):
            items = [str(item) for item in value]
        else:
            parts.append(f"{name}: {value!r}")
            continue
        tail = items[-TAIL_ITEMS:]
        header = f"{name} (last {len(tail)} of {len(items)}):"
        parts.append("\n  ".join([header, *tail]))
    return "".join(f"\n{part}" for part in parts)


async def wait_or_fail[T](
    aw: Awaitable[T] | asyncio.Event,
    timeout: float = 5.0,
    msg: str = "timed out",
    **context: object,
) -> T:
    """Await ``aw``, or wait for an ``asyncio.Event`` to be set, and fail the test on timeout.

    Behaves like ``asyncio.wait_for``, so a timed out future is cancelled; a TimeoutError raised
    by ``aw`` itself propagates unchanged. ``context`` values are formatted only on failure, so
    pass live collections; lists, tuples, deques and mappings show their last 20 items.
    """
    if isinstance(aw, asyncio.Event):
        aw = aw.wait()
    deadline = asyncio.timeout(timeout)
    try:
        async with deadline:
            return await aw
    except TimeoutError:
        # A TimeoutError raised by aw itself is not this wait running out
        if not deadline.expired():
            raise
    # Failing outside the except block keeps the TimeoutError traceback out of the report.
    pytest.fail(f"{msg} (after {timeout}s){_format_context(context)}")
