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
import logging
import threading
from typing import cast

_LOGGER = logging.getLogger(__name__)


class AsyncThreadRunner[T](threading.Thread):
    """Run an async coroutine in a daemon thread and expose its result.

    The runner catches all exceptions from the coroutine and stores them in
    ``exception`` so ``event`` is always set — this prevents callers waiting
    on ``event`` from hanging forever when the coroutine crashes.

    Prefer :func:`run_async` for the common start/wait/raise sequence; use
    this class directly only when partial results are needed after a timeout
    (see ``esphome/zeroconf.py``).
    """

    def __init__(self, coro_factory: Callable[[], Awaitable[T]]) -> None:
        super().__init__(daemon=True)
        self._coro_factory = coro_factory
        self.result: T | None = None
        self.exception: BaseException | None = None
        self.completed = False
        self.event = threading.Event()

    async def _runner(self) -> None:
        try:
            self.result = await self._coro_factory()
            # Distinguishes a delivered result from "never ran", since None
            # is a valid result value.
            self.completed = True
        except BaseException as exc:  # noqa: BLE001  # pylint: disable=broad-except
            # Capture everything, including BaseException — otherwise a
            # cancellation or SystemExit would leave ``exception`` unset and
            # waiters would mistake the empty ``result`` for success.
            self.exception = exc
        finally:
            self.event.set()

    def run(self) -> None:
        try:
            asyncio.run(self._runner())
        except BaseException as exc:  # noqa: BLE001  # pylint: disable=broad-except
            # asyncio.run itself can fail before _runner executes (e.g. loop
            # creation under fd exhaustion); record it so waiters never hang.
            # A failure during loop cleanup after the coroutine completed
            # must not clobber the delivered result, hence the guard.
            if self.exception is None and not self.completed:
                self.exception = exc
            else:
                _LOGGER.debug(
                    "Event loop teardown failed after result delivered",
                    exc_info=True,
                )
        finally:
            self.event.set()


def run_async[T](
    coro_factory: Callable[[], Awaitable[T]],
    timeout: float | None = None,
    on_orphan: Callable[[T], None] | None = None,
) -> T:
    """Run a coroutine in a daemon-thread event loop and return its result.

    The result is available as soon as the coroutine completes; the loop's
    cleanup cycle finishes in the background. Raises ``TimeoutError`` when
    the coroutine does not complete within ``timeout`` seconds (the daemon
    thread is abandoned and exits with the interpreter). If ``on_orphan`` is
    given it is called with the result should the abandoned thread produce
    one after the timeout, so resources (e.g. a connected socket) can be
    released instead of leaking.
    """
    runner: AsyncThreadRunner[T] = AsyncThreadRunner(coro_factory)
    runner.start()
    if not runner.event.wait(timeout):

        def _cleanup() -> None:
            runner.event.wait()
            if not runner.completed:
                # The only place an abandoned thread's real error surfaces;
                # without it a late failure hides behind the TimeoutError.
                _LOGGER.debug("Abandoned async operation failed: %s", runner.exception)
                return
            if on_orphan is None or (result := runner.result) is None:
                return
            try:
                on_orphan(result)
            except Exception:  # pylint: disable=broad-except
                _LOGGER.debug("Error releasing orphaned result", exc_info=True)

        threading.Thread(
            target=_cleanup, daemon=True, name="async-orphan-cleanup"
        ).start()
        raise TimeoutError("Timed out waiting for async operation")
    if (exc := runner.exception) is not None:
        raise exc
    return cast("T", runner.result)
