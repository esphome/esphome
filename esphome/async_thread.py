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
from itertools import count
import logging
import threading
from typing import cast

_LOGGER = logging.getLogger(__name__)

# How long the orphan watcher waits for an abandoned coroutine before giving
# up, so a hung operation does not park a watcher thread forever.
ORPHAN_WAIT_TIMEOUT = 300.0


_runner_ids = count(1)


class AsyncDispatchTimeout(TimeoutError):
    """The caller stopped waiting; the coroutine was abandoned.

    A subclass so callers can tell the dispatcher's own expiry apart from a
    ``TimeoutError`` raised inside the coroutine, while existing
    ``except TimeoutError`` handlers keep working.
    """


class AsyncThreadRunner[T](threading.Thread):
    """Run an async coroutine in a daemon thread and expose its result.

    ``event`` is always set, even when the coroutine crashes, so waiters
    never hang; ``completed`` distinguishes a delivered result (even a
    legitimate ``None``) from a coroutine that never finished. Prefer
    :func:`run_async`; use this class directly only when a failure should
    degrade to a default value instead of raising.
    """

    def __init__(self, coro_factory: Callable[[], Awaitable[T]]) -> None:
        super().__init__(daemon=True, name=f"async-thread-runner-{next(_runner_ids)}")
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
                    "Event loop teardown failed after outcome recorded",
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

    Raises :class:`AsyncDispatchTimeout` if the coroutine does not finish
    within ``timeout`` seconds; the thread is abandoned and exits with the
    interpreter. If the abandoned coroutine later produces a result,
    ``on_orphan`` (if given) is called with it so resources such as a
    connected socket can be released; delivery is best effort and bounded
    by ``ORPHAN_WAIT_TIMEOUT``.
    """
    runner: AsyncThreadRunner[T] = AsyncThreadRunner(coro_factory)
    runner.start()
    if not runner.event.wait(timeout):

        def _cleanup() -> None:
            if not runner.event.wait(ORPHAN_WAIT_TIMEOUT):
                # The one state where a resource can genuinely leak; leave
                # a trace so a recurring hang is attributable.
                _LOGGER.info(
                    "Orphan watcher gave up after %.0fs; a late result may leak",
                    ORPHAN_WAIT_TIMEOUT,
                )
                return
            if not runner.completed:
                # The only place an abandoned thread's real error surfaces;
                # without it a late failure hides behind the TimeoutError.
                # INFO, not DEBUG: it fires at most once per abandoned
                # operation and the cause may not reproduce on a rerun.
                _LOGGER.info(
                    "Abandoned async operation failed",
                    exc_info=runner.exception,
                )
                return
            if (result := runner.result) is None:
                return
            if on_orphan is None:
                _LOGGER.debug("Discarding late result; no on_orphan handler")
                return
            try:
                on_orphan(result)
            except Exception:  # pylint: disable=broad-except
                # INFO, not DEBUG: a failed release means a real leak, and
                # it fires at most once per abandoned operation.
                _LOGGER.info("Error releasing orphaned result", exc_info=True)

        threading.Thread(
            target=_cleanup, daemon=True, name="async-orphan-cleanup"
        ).start()
        raise AsyncDispatchTimeout("Timed out waiting for async operation")
    if (exc := runner.exception) is not None:
        raise exc
    if not runner.completed:
        raise RuntimeError("Async operation finished without a result or an exception")
    return cast("T", runner.result)
