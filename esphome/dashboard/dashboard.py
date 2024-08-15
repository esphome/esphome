from __future__ import annotations

import asyncio
from asyncio import events
from concurrent.futures import ThreadPoolExecutor
import logging
import os
import socket
import threading
from time import monotonic
import traceback
from typing import Any

from esphome.storage_json import EsphomeStorageJSON, esphome_storage_path

from .const import MAX_EXECUTOR_WORKERS
from .core import DASHBOARD
from .web_server import make_app, start_web_server

ENV_DEV = "ESPHOME_DASHBOARD_DEV"

settings = DASHBOARD.settings


def can_use_pidfd() -> bool:
    """Check if pidfd_open is available.

    Back ported from cpython 3.12
    """
    if not hasattr(os, "pidfd_open"):
        return False
    try:
        pid = os.getpid()
        os.close(os.pidfd_open(pid, 0))
    except OSError:
        # blocked by security policy like SECCOMP
        return False
    return True


class DashboardEventLoopPolicy(asyncio.DefaultEventLoopPolicy):
    """Event loop policy for Home Assistant."""

    def __init__(self, debug: bool) -> None:
        """Init the event loop policy."""
        super().__init__()
        self.debug = debug
        self._watcher: asyncio.AbstractChildWatcher | None = None

    def _init_watcher(self) -> None:
        """Initialize the watcher for child processes.

        Back ported from cpython 3.12
        """
        with events._lock:  # type: ignore[attr-defined] # pylint: disable=protected-access
            if self._watcher is None:  # pragma: no branch
                if can_use_pidfd():
                    self._watcher = asyncio.PidfdChildWatcher()
                else:
                    self._watcher = asyncio.ThreadedChildWatcher()
                if threading.current_thread() is threading.main_thread():
                    self._watcher.attach_loop(
                        self._local._loop  # type: ignore[attr-defined] # pylint: disable=protected-access
                    )

    @property
    def loop_name(self) -> str:
        """Return name of the loop."""
        return self._loop_factory.__name__  # type: ignore[no-any-return,attr-defined]

    def new_event_loop(self) -> asyncio.AbstractEventLoop:
        """Get the event loop."""
        loop: asyncio.AbstractEventLoop = super().new_event_loop()
        loop.set_exception_handler(_async_loop_exception_handler)

        if self.debug:
            loop.set_debug(True)

        executor = ThreadPoolExecutor(
            thread_name_prefix="SyncWorker", max_workers=MAX_EXECUTOR_WORKERS
        )
        loop.set_default_executor(executor)
        # bind the built-in time.monotonic directly as loop.time to avoid the
        # overhead of the additional method call since its the most called loop
        # method and its roughly 10%+ of all the call time in base_events.py
        loop.time = monotonic  # type: ignore[method-assign]
        return loop


def _async_loop_exception_handler(_: Any, context: dict[str, Any]) -> None:
    """Handle all exception inside the core loop."""
    kwargs = {}
    if exception := context.get("exception"):
        kwargs["exc_info"] = (type(exception), exception, exception.__traceback__)

    logger = logging.getLogger(__package__)
    if source_traceback := context.get("source_traceback"):
        stack_summary = "".join(traceback.format_list(source_traceback))
        logger.error(
            "Error doing job: %s: %s",
            context["message"],
            stack_summary,
            **kwargs,  # type: ignore[arg-type]
        )
        return

    logger.error(
        "Error doing job: %s",
        context["message"],
        **kwargs,  # type: ignore[arg-type]
    )


def start_dashboard(args) -> None:
    """Start the dashboard."""
    settings.parse_args(args)

    if settings.using_auth:
        path = esphome_storage_path()
        storage = EsphomeStorageJSON.load(path)
        if storage is None:
            storage = EsphomeStorageJSON.get_default()
            storage.save(path)
        settings.cookie_secret = storage.cookie_secret

    asyncio.set_event_loop_policy(DashboardEventLoopPolicy(settings.verbose))

    try:
        asyncio.run(async_start(args))
    except KeyboardInterrupt:
        pass


async def async_start(args) -> None:
    """Start the dashboard."""
    dashboard = DASHBOARD
    await dashboard.async_setup()
    sock: socket.socket | None = args.socket
    address: str | None = args.address
    port: int | None = args.port

    start_web_server(make_app(args.verbose), sock, address, port, settings.config_dir)

    if args.open_ui:
        import webbrowser

        webbrowser.open(f"http://{args.address}:{args.port}")

    try:
        await dashboard.async_run()
    finally:
        if sock:
            os.remove(sock)
