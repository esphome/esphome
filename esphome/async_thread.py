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
# up; a coroutine that has not finished by then has no result worth releasing,
# and an unbounded wait would park a second thread per hung operation for the
# process lifetime (which adds up in long-lived hosts like the dashboard).
ORPHAN_WAIT_TIMEOUT = 300.0


# Cap on concurrently parked orphan watchers; repeated hangs otherwise
# accumulate a watcher (up to ORPHAN_WAIT_TIMEOUT each) per abandoned
# operation. Past the cap the late result is knowingly dropped instead.
_MAX_ORPHAN_WATCHERS = 8
_active_watchers = 0
_watcher_lock = threading.Lock()

_runner_ids = count(1)


class AsyncDispatchTimeout(TimeoutError):
    """The caller stopped waiting; the coroutine was abandoned.

    A subclass so callers can tell the dispatcher's own expiry apart from a
    ``TimeoutError`` raised inside the coroutine, while existing
    ``except TimeoutError`` handlers keep working.
    """


class AsyncThreadRunner[T](threading.Thread):
    """Run an async coroutine in a daemon thread and expose its result.

    The runner captures ``BaseException`` from the coroutine and stores it
    in ``exception`` so ``event`` is always set — this prevents callers
    waiting on ``event`` from hanging forever when the coroutine crashes.
    ``completed`` distinguishes a delivered result (even a legitimate
    ``None``) from a coroutine that never finished.

    Prefer :func:`run_async` for the common start/wait/raise sequence; use
    this class directly only when a timeout or failure should degrade to a
    default value with a log message instead of raising (see
    ``esphome/zeroconf.py``).
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

    The result is available as soon as the coroutine completes; the loop's
    cleanup cycle finishes in the background. Raises
    :class:`AsyncDispatchTimeout` (a ``TimeoutError`` subclass, so it stays
    distinguishable from a timeout raised inside the coroutine) when the
    coroutine does not complete within ``timeout`` seconds; the daemon
    thread is abandoned and exits with the interpreter. If ``on_orphan`` is
    given it is called with the result should the abandoned thread produce
    one after the timeout, so resources (e.g. a connected socket) can be
    released instead of leaking. Delivery is best effort and bounded by
    ``ORPHAN_WAIT_TIMEOUT``; a ``None`` result is skipped, since there is
    nothing to release.
    """
    runner: AsyncThreadRunner[T] = AsyncThreadRunner(coro_factory)
    runner.start()
    if not runner.event.wait(timeout):
        global _active_watchers  # noqa: PLW0603

        def _cleanup() -> None:
            global _active_watchers  # noqa: PLW0603
            try:
                _watch_for_orphan()
            finally:
                with _watcher_lock:
                    _active_watchers -= 1

        def _watch_for_orphan() -> None:
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

        with _watcher_lock:
            spawn_watcher = _active_watchers < _MAX_ORPHAN_WATCHERS
            if spawn_watcher:
                _active_watchers += 1
        if spawn_watcher:
            threading.Thread(
                target=_cleanup, daemon=True, name="async-orphan-cleanup"
            ).start()
        else:
            _LOGGER.info("Too many pending orphan watchers; a late result may leak")
        raise AsyncDispatchTimeout("Timed out waiting for async operation")
    if (exc := runner.exception) is not None:
        raise exc
    if not runner.completed:
        raise RuntimeError("Async operation finished without a result or an exception")
    return cast("T", runner.result)
