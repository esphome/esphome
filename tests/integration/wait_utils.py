"""Helpers for waiting on futures and events in integration tests."""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable

import pytest

# How many trailing items of a list are shown in a failure message.
TAIL_ITEMS = 20


def _format_context(context: dict[str, object]) -> str:
    """Render context values for a failure message, keeping only the tail of lists."""
    parts: list[str] = []
    for name, value in context.items():
        if isinstance(value, list):
            tail = value[-TAIL_ITEMS:]
            header = f"{name} (last {len(tail)} of {len(value)}):"
            parts.append("\n  ".join([header, *map(str, tail)]))
        else:
            parts.append(f"{name}: {value!r}")
    return "".join(f"\n{part}" for part in parts)


async def wait_or_fail[T](
    aw: Awaitable[T] | asyncio.Event,
    timeout: float = 5.0,
    msg: str = "timed out",
    **context: object,
) -> T:
    """Await ``aw``, or wait for an ``asyncio.Event`` to be set, and fail the test on timeout.

    Behaves like ``asyncio.wait_for``, so a timed out future is cancelled. ``context`` values
    are formatted only on failure, so pass live lists and dicts; lists show their last 20 items.
    """
    if isinstance(aw, asyncio.Event):
        aw = aw.wait()
    try:
        return await asyncio.wait_for(aw, timeout)
    except TimeoutError:
        pass
    # Failing outside the except block keeps the TimeoutError traceback out of the report.
    pytest.fail(f"{msg} (after {timeout}s){_format_context(context)}")
