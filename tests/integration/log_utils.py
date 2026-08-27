"""Helpers for asserting on log output in integration tests."""

from __future__ import annotations

import asyncio


class LineWaiter:
    """Collects log lines and lets a test await one containing all needles.

    Pass ``callback`` as ``run_compiled``'s ``line_callback``; the callback runs
    on the test's own event loop, so futures are resolved directly. Only one
    ``wait_for`` may be outstanding at a time (tests await sequentially).
    """

    def __init__(self) -> None:
        self.lines: list[str] = []
        self._needles: tuple[str, ...] = ()
        self._future: asyncio.Future | None = None

    def callback(self, line: str) -> None:
        self.lines.append(line)
        if (
            self._future is not None
            and not self._future.done()
            and all(n in line for n in self._needles)
        ):
            self._future.set_result(line)
            self._future = None

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
