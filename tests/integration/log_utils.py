"""Helpers for asserting on log output in integration tests."""

from __future__ import annotations

import asyncio
import re
from typing import Any, overload

from .wait_utils import wait_or_fail

type LinePattern = str | re.Pattern[str]
type LineMatch = str | re.Match[str]


def _match(pattern: LinePattern, line: str) -> LineMatch | None:
    """Return ``line`` if a str pattern is in it, or the search match of a regex."""
    if isinstance(pattern, str):
        return line if pattern in line else None
    return pattern.search(line)


class LineWaiter:
    """Collects log lines and lets a test await lines that match.

    Pass ``callback`` as ``run_compiled``'s ``line_callback``; the callback runs
    on the test's own event loop, so futures are resolved directly. ``wait_for``
    also matches lines seen before the call, and only one may be outstanding at a
    time. ``expect`` matches only later lines, and any number may be armed at once;
    each resolves on the next matching line, so two armed for the same text both
    resolve on its first occurrence. Arm the second after the first, or use
    ``wait_count`` to wait for the Nth matching line.
    """

    def __init__(self) -> None:
        self.lines: list[str] = []
        self._needles: tuple[str, ...] = ()
        self._future: asyncio.Future | None = None
        self._expectations: dict[asyncio.Future[Any], LinePattern] = {}

    def callback(self, line: str) -> None:
        self.lines.append(line)
        for future, pattern in list(self._expectations.items()):
            if future.done():
                del self._expectations[future]
            elif (match := _match(pattern, line)) is not None:
                future.set_result(match)
                del self._expectations[future]
        if (
            self._future is not None
            and not self._future.done()
            and all(n in line for n in self._needles)
        ):
            self._future.set_result(line)
            self._future = None

    async def wait_for_each(self, *texts: str, timeout: float = 10.0) -> None:
        """Await each text in turn; a text may match a line already received."""
        for text in texts:
            await self.wait_for(text, timeout=timeout)

    async def wait_for(self, *needles: str, timeout: float = 10.0) -> str:
        """Return the first line, past or future, containing every needle."""
        for line in self.lines:
            if all(n in line for n in needles):
                return line
        assert self._future is None or self._future.done(), "concurrent wait_for"
        self._needles = needles
        self._future = asyncio.get_running_loop().create_future()
        try:
            return await asyncio.wait_for(self._future, timeout)
        finally:
            self._future = None
            self._needles = ()

    @overload
    def expect(self, pattern: str) -> asyncio.Future[str]: ...  # noqa: E704

    @overload
    def expect(self, pattern: re.Pattern[str]) -> asyncio.Future[re.Match[str]]: ...  # noqa: E704

    def expect(self, pattern: LinePattern) -> asyncio.Future[LineMatch]:
        """Arm a wait for the next line matching ``pattern``; earlier lines never match.

        A str matches as a substring and resolves to the line; a compiled regex is
        searched and resolves to its match. Arm it before the action that logs the line.
        """
        future: asyncio.Future[LineMatch] = asyncio.get_running_loop().create_future()
        self._expectations[future] = pattern
        return future

    async def wait[T: str | re.Match[str]](
        self,
        future: asyncio.Future[T],
        timeout: float = 5.0,
        msg: str = "log line not seen",
    ) -> T:
        """Await a future from ``expect``; on timeout fail with its pattern and the last lines."""
        return await wait_or_fail(
            future,
            timeout,
            msg,
            pattern=self._expectations.get(future),
            lines=self.lines,
        )

    async def wait_count(
        self,
        pattern: LinePattern,
        n: int,
        timeout: float = 5.0,
        msg: str = "log line not repeated",
    ) -> None:
        """Wait until ``n`` lines, past or future, match ``pattern``; fail like ``wait``."""

        async def reach() -> None:
            while self.count(pattern) < n:
                await self.expect(pattern)

        await wait_or_fail(
            reach(),
            timeout,
            msg,
            pattern=pattern,
            seen=lambda: f"{self.count(pattern)} of {n}",
            lines=self.lines,
        )

    def count(self, pattern: LinePattern) -> int:
        """Return how many lines seen so far match ``pattern``."""
        return sum(_match(pattern, line) is not None for line in self.lines)
