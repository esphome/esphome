"""Helpers for running an async coroutine from sync code via a daemon thread.

``asyncio.run(coro())`` in the main thread blocks until the loop's cleanup
cycle finishes, which can add hundreds of milliseconds before the caller
receives the result. Running the loop in a daemon thread lets the caller
observe the result as soon as the coroutine completes while cleanup finishes
in the background.
"""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable, Callable
import threading
from typing import Generic, TypeVar

_T = TypeVar("_T")


class AsyncThreadRunner(threading.Thread, Generic[_T]):
    """Run an async coroutine in a daemon thread and expose its result.

    The runner catches all exceptions from the coroutine and stores them in
    ``exception`` so ``event`` is always set — this prevents callers waiting
    on ``event`` from hanging forever when the coroutine crashes.

    Typical usage::

        runner = AsyncThreadRunner(lambda: my_coro(arg))
        runner.start()
        if not runner.event.wait(timeout=5.0):
            ...  # timed out
        if runner.exception is not None:
            raise runner.exception
        result = runner.result
    """

    def __init__(self, coro_factory: Callable[[], Awaitable[_T]]) -> None:
        super().__init__(daemon=True)
        self._coro_factory = coro_factory
        self.result: _T | None = None
        self.exception: BaseException | None = None
        self.event = threading.Event()

    async def _runner(self) -> None:
        try:
            self.result = await self._coro_factory()
        except Exception as exc:  # pylint: disable=broad-except
            # Capture all exceptions so ``event`` is always set — otherwise a
            # crash would hang the waiter forever.
            self.exception = exc
        finally:
            self.event.set()

    def run(self) -> None:
        asyncio.run(self._runner())
