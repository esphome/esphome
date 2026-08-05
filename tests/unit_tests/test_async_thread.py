"""Tests for the async thread helpers."""

from __future__ import annotations

import asyncio
import threading
import time
from typing import Any
from unittest.mock import patch

import pytest

from esphome.async_thread import AsyncThreadRunner, run_async


def _cleanup_threads() -> set[threading.Thread]:
    """Return the currently live orphan-cleanup threads."""
    return {t for t in threading.enumerate() if t.name == "async-orphan-cleanup"}


def _join_new_cleanup_threads(before: set[threading.Thread]) -> None:
    """Wait for cleanup threads spawned since ``before`` to finish."""
    for thread in _cleanup_threads() - before:
        thread.join(5)
        assert not thread.is_alive()


def test_run_async_returns_result() -> None:
    """The coroutine's result is returned to the sync caller."""

    async def coro() -> int:
        await asyncio.sleep(0)
        return 42

    assert run_async(coro) == 42


def test_run_async_propagates_exception() -> None:
    """Exceptions raised by the coroutine surface in the caller."""

    async def coro() -> None:
        raise ValueError("boom")

    with pytest.raises(ValueError, match="boom"):
        run_async(coro)


def test_run_async_propagates_base_exception() -> None:
    """A BaseException from the coroutine surfaces instead of a None result."""

    class Boom(BaseException):
        pass

    async def coro() -> None:
        raise Boom

    with pytest.raises(Boom):
        run_async(coro)


def test_run_async_timeout() -> None:
    """A coroutine that does not finish in time raises TimeoutError."""
    release = threading.Event()

    async def coro() -> None:
        await asyncio.get_running_loop().run_in_executor(None, release.wait)

    with pytest.raises(TimeoutError):
        run_async(coro, timeout=0.05)

    # Unblock the abandoned runner so its cleanup thread exits promptly.
    release.set()


def test_run_async_surfaces_loop_startup_failure() -> None:
    """A failure before the coroutine runs raises instead of hanging."""
    with (
        patch(
            "esphome.async_thread.asyncio.run",
            side_effect=OSError("no fds for the event loop"),
        ),
        pytest.raises(OSError, match="no fds"),
    ):
        run_async(lambda: asyncio.sleep(0), timeout=5)


def test_run_preserves_result_when_cleanup_fails(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A loop-cleanup failure after success is logged, not raised."""

    async def coro() -> str:
        return "ok"

    runner: AsyncThreadRunner[str] = AsyncThreadRunner(coro)

    def fake_run(main: Any) -> None:
        main.close()
        # Emulate _runner delivering the result before cleanup raised. A
        # None result must count as delivered too, hence the completed flag.
        runner.result = "ok"
        runner.completed = True
        raise KeyboardInterrupt

    with (
        caplog.at_level("DEBUG", logger="esphome.async_thread"),
        patch("esphome.async_thread.asyncio.run", side_effect=fake_run),
    ):
        runner.run()

    assert runner.event.is_set()
    assert runner.exception is None
    assert runner.result == "ok"
    assert "teardown failed after result delivered" in caplog.text


def test_run_async_none_result_is_success() -> None:
    """A coroutine legitimately returning None is not treated as a failure."""

    async def coro() -> None:
        return None

    assert run_async(coro) is None


def test_run_async_on_orphan_skips_none_result() -> None:
    """A late None result completes cleanly without invoking on_orphan."""
    orphaned: list[Any] = []
    finished = threading.Event()
    release = threading.Event()

    async def coro() -> None:
        await asyncio.get_running_loop().run_in_executor(None, release.wait)
        finished.set()

    before = _cleanup_threads()
    with pytest.raises(TimeoutError):
        run_async(coro, timeout=0.01, on_orphan=orphaned.append)

    release.set()
    assert finished.wait(5)
    _join_new_cleanup_threads(before)
    assert not orphaned


def test_late_failure_without_on_orphan_is_logged(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An abandoned thread's real error leaves a debug trace."""
    release = threading.Event()

    async def coro() -> str:
        await asyncio.get_running_loop().run_in_executor(None, release.wait)
        raise ValueError("the real cause")

    before = _cleanup_threads()
    with caplog.at_level("DEBUG", logger="esphome.async_thread"):
        with pytest.raises(TimeoutError):
            run_async(coro, timeout=0.01)

        release.set()
        _join_new_cleanup_threads(before)
        assert "Abandoned async operation failed" in caplog.text
        assert "the real cause" in caplog.text


def test_run_async_on_orphan_failure_is_contained(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An on_orphan callback that raises is logged, not propagated."""
    released = threading.Event()
    release = threading.Event()

    def on_orphan(result: str) -> None:
        released.set()
        raise OSError("close failed")

    async def coro() -> str:
        await asyncio.get_running_loop().run_in_executor(None, release.wait)
        return "late result"

    with caplog.at_level("DEBUG", logger="esphome.async_thread"):
        with pytest.raises(TimeoutError):
            run_async(coro, timeout=0.01, on_orphan=on_orphan)

        release.set()
        assert released.wait(5)
        deadline = time.monotonic() + 5
        while (
            "Error releasing orphaned result" not in caplog.text
            and time.monotonic() < deadline
        ):
            time.sleep(0.01)
        assert "Error releasing orphaned result" in caplog.text


def test_run_async_on_orphan_releases_late_result() -> None:
    """A result produced after the timeout is handed to on_orphan."""
    orphaned: list[Any] = []
    delivered = threading.Event()
    release = threading.Event()

    def on_orphan(result: str) -> None:
        orphaned.append(result)
        delivered.set()

    async def coro() -> str:
        # Block until the test has observed the timeout, so the result is
        # guaranteed to arrive late no matter how slowly the runner is
        # scheduled.
        await asyncio.get_running_loop().run_in_executor(None, release.wait)
        return "late result"

    with pytest.raises(TimeoutError):
        run_async(coro, timeout=0.01, on_orphan=on_orphan)

    release.set()
    assert delivered.wait(5)
    assert orphaned == ["late result"]


def test_run_async_on_orphan_skips_late_failure() -> None:
    """A late failure after the timeout is not handed to on_orphan."""
    orphaned: list[Any] = []
    failed = threading.Event()
    release = threading.Event()

    async def coro() -> str:
        # Block until the test has observed the timeout, so the failure is
        # guaranteed to arrive late.
        await asyncio.get_running_loop().run_in_executor(None, release.wait)
        failed.set()
        raise ValueError("late failure")

    before = _cleanup_threads()
    with pytest.raises(TimeoutError):
        run_async(coro, timeout=0.01, on_orphan=orphaned.append)

    release.set()
    assert failed.wait(5)
    _join_new_cleanup_threads(before)
    assert not orphaned
