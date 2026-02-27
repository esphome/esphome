from __future__ import annotations

import asyncio
from collections.abc import Callable, Coroutine
import contextlib
from dataclasses import dataclass
from functools import partial
import json
import logging
from pathlib import Path
import threading
from typing import Any

from esphome import const
from esphome.storage_json import StorageJSON, ext_storage_path, ignored_devices_storage_path

from ..zeroconf import DiscoveredImport
from .const import DASHBOARD_COMMAND, DashboardEvent
from .dns import DNSCache
from .entries import DashboardEntries, DashboardEntry
from .settings import DashboardSettings
from .status.mdns import MDNSStatus
from .status.ping import PingStatus
from .util.subprocess import async_run_system_command

_LOGGER = logging.getLogger(__name__)

IGNORED_DEVICES_STORAGE_PATH = "ignored-devices.json"

MDNS_BOOTSTRAP_TIME = 7.5


@dataclass
class Event:
    """Dashboard Event."""

    event_type: DashboardEvent
    data: dict[str, Any]


class EventBus:
    """Dashboard event bus."""

    def __init__(self) -> None:
        """Initialize the Dashboard event bus."""
        self._listeners: dict[DashboardEvent, set[Callable[[Event], None]]] = {}

    def async_add_listener(
        self, event_type: DashboardEvent, listener: Callable[[Event], None]
    ) -> Callable[[], None]:
        """Add a listener to the event bus."""
        self._listeners.setdefault(event_type, set()).add(listener)
        return partial(self._async_remove_listener, event_type, listener)

    def _async_remove_listener(
        self, event_type: DashboardEvent, listener: Callable[[Event], None]
    ) -> None:
        """Remove a listener from the event bus."""
        self._listeners[event_type].discard(listener)

    def async_fire(
        self, event_type: DashboardEvent, event_data: dict[str, Any]
    ) -> None:
        """Fire an event."""
        event = Event(event_type, event_data)

        _LOGGER.debug("Firing event: %s", event)

        for listener in self._listeners.get(event_type, set()):
            listener(event)


def _restore_storage_version(entry: DashboardEntry, old_version: str) -> None:
    """Restore the esphome_version in the storage JSON after a failed build.

    ``esphome compile`` writes the storage JSON during code generation (before
    PlatformIO runs).  If the compile ultimately fails we must roll the version
    back so the next server restart retries the build.
    """
    storage_path = ext_storage_path(entry.filename)
    try:
        with storage_path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
        if data.get("esphome_version") != old_version:
            data["esphome_version"] = old_version
            with storage_path.open("w", encoding="utf-8") as fh:
                json.dump(data, fh, indent=2)
                fh.write("\n")
            entry.load_from_disk()
    except (OSError, json.JSONDecodeError):
        _LOGGER.debug("Could not restore storage version for %s", entry.filename)


def _cleanup_old_firmware(entry: DashboardEntry, old_firmware_path: Path | None) -> None:
    """Delete the previous firmware binary after a successful re-compile.

    After a version upgrade the compiler may produce a binary at a new path.
    This removes the stale binary from the previous version to reclaim storage.
    """
    if old_firmware_path is None:
        return
    try:
        storage = StorageJSON.load(ext_storage_path(entry.filename))
        if storage is None or storage.firmware_bin_path is None:
            return
        if storage.firmware_bin_path == old_firmware_path:
            return
        if old_firmware_path.is_file():
            old_firmware_path.unlink()
            _LOGGER.debug(
                "Removed old firmware binary for %s: %s",
                entry.filename,
                old_firmware_path,
            )
    except Exception:  # pylint: disable=broad-except
        _LOGGER.debug(
            "Could not clean up old firmware for %s", entry.filename, exc_info=True
        )


class ESPHomeDashboard:
    """Class that represents the dashboard."""

    __slots__ = (
        "bus",
        "entries",
        "loop",
        "import_result",
        "stop_event",
        "ping_request",
        "mqtt_ping_request",
        "mdns_status",
        "settings",
        "dns_cache",
        "_background_tasks",
        "ignored_devices",
        "_ping_status_task",
    )

    def __init__(self) -> None:
        """Initialize the ESPHomeDashboard."""
        self.bus = EventBus()
        self.entries: DashboardEntries | None = None
        self.loop: asyncio.AbstractEventLoop | None = None
        self.import_result: dict[str, DiscoveredImport] = {}
        self.stop_event = threading.Event()
        self.ping_request: asyncio.Event | None = None
        self.mqtt_ping_request = threading.Event()
        self.mdns_status: MDNSStatus | None = None
        self.settings = DashboardSettings()
        self.dns_cache = DNSCache()
        self._background_tasks: set[asyncio.Task] = set()
        self.ignored_devices: set[str] = set()
        self._ping_status_task: asyncio.Task | None = None

    async def async_setup(self) -> None:
        """Setup the dashboard."""
        self.loop = asyncio.get_running_loop()
        self.ping_request = asyncio.Event()
        self.entries = DashboardEntries(self)
        await self.loop.run_in_executor(None, self.load_ignored_devices)

    def load_ignored_devices(self) -> None:
        storage_path = ignored_devices_storage_path()
        try:
            with storage_path.open("r", encoding="utf-8") as f_handle:
                data = json.load(f_handle)
                self.ignored_devices = set(data.get("ignored_devices", set()))
        except FileNotFoundError:
            pass

    def save_ignored_devices(self) -> None:
        storage_path = ignored_devices_storage_path()
        with storage_path.open("w", encoding="utf-8") as f_handle:
            json.dump(
                {"ignored_devices": sorted(self.ignored_devices)}, indent=2, fp=f_handle
            )

    def _async_start_ping_status(self, ping_status: PingStatus) -> None:
        self._ping_status_task = asyncio.create_task(ping_status.async_run())

    async def _async_prebuild_devices(self) -> None:
        """Pre-build firmware for devices that need a version update."""
        entries = self.entries.async_all()
        devices_to_build = [entry for entry in entries if entry.update_available]

        if not devices_to_build:
            _LOGGER.info(
                "All devices already built for version %s, skipping pre-build",
                const.__version__,
            )
            return

        total = len(devices_to_build)
        _LOGGER.info(
            "Pre-building firmware for %d device(s) (version %s)",
            total,
            const.__version__,
        )
        self.bus.async_fire(
            DashboardEvent.PRE_BUILD_STATUS,
            {"status": "started", "total": total, "version": const.__version__},
        )

        succeeded = 0
        failed = 0
        for idx, entry in enumerate(devices_to_build, start=1):
            config_path = str(self.settings.rel_path(entry.filename))
            old_version = entry.update_old
            # Record the current firmware path so we can clean it up after
            # a successful re-compile produces a binary at a new path.
            old_storage = StorageJSON.load(ext_storage_path(entry.filename))
            old_firmware_path = (
                old_storage.firmware_bin_path if old_storage is not None else None
            )
            _LOGGER.info("Pre-building %s (%d/%d)", entry.name, idx, total)
            try:
                returncode, _stdout, stderr = await async_run_system_command(
                    [*DASHBOARD_COMMAND, "compile", config_path]
                )
                if returncode == 0:
                    succeeded += 1
                    # Refresh cached storage so update_available becomes False
                    await self.loop.run_in_executor(None, entry.load_from_disk)
                    await self.loop.run_in_executor(
                        None,
                        _cleanup_old_firmware,
                        entry,
                        old_firmware_path,
                    )
                    self.bus.async_fire(
                        DashboardEvent.PRE_BUILD_STATUS,
                        {
                            "status": "device_done",
                            "filename": entry.filename,
                            "name": entry.name,
                            "current": idx,
                            "total": total,
                        },
                    )
                    _LOGGER.info("Pre-build succeeded for %s", entry.name)
                else:
                    failed += 1
                    error_msg = stderr.decode(errors="replace")[-500:]
                    # esphome compile updates the storage JSON during code
                    # generation before PlatformIO runs.  If the compile
                    # ultimately fails we must restore the old version so the
                    # next server restart retries the build.
                    await self.loop.run_in_executor(
                        None,
                        _restore_storage_version,
                        entry,
                        old_version,
                    )
                    self.bus.async_fire(
                        DashboardEvent.PRE_BUILD_STATUS,
                        {
                            "status": "device_failed",
                            "filename": entry.filename,
                            "name": entry.name,
                            "error": error_msg,
                            "current": idx,
                            "total": total,
                        },
                    )
                    _LOGGER.warning(
                        "Pre-build failed for %s: %s", entry.name, error_msg
                    )
            except Exception:  # pylint: disable=broad-except
                failed += 1
                _LOGGER.exception("Pre-build error for %s", entry.name)
                await self.loop.run_in_executor(
                    None,
                    _restore_storage_version,
                    entry,
                    old_version,
                )
                self.bus.async_fire(
                    DashboardEvent.PRE_BUILD_STATUS,
                    {
                        "status": "device_failed",
                        "filename": entry.filename,
                        "name": entry.name,
                        "error": "Unexpected error",
                        "current": idx,
                        "total": total,
                    },
                )

        self.bus.async_fire(
            DashboardEvent.PRE_BUILD_STATUS,
            {
                "status": "finished",
                "total": total,
                "succeeded": succeeded,
                "failed": failed,
            },
        )
        _LOGGER.info("Pre-build complete: %d succeeded, %d failed", succeeded, failed)

    async def async_run(self) -> None:
        """Run the dashboard."""
        settings = self.settings
        mdns_task: asyncio.Task | None = None
        await self.entries.async_update_entries()

        if settings.auto_build:
            self.async_create_background_task(self._async_prebuild_devices())
        else:
            _LOGGER.info("Automatic firmware pre-build is disabled")

        mdns_status = MDNSStatus(self)
        ping_status = PingStatus(self)
        start_ping_timer: asyncio.TimerHandle | None = None

        self.mdns_status = mdns_status
        if mdns_status.async_setup():
            mdns_task = asyncio.create_task(mdns_status.async_run())
            # Start ping MDNS_BOOTSTRAP_TIME seconds after startup to ensure
            # MDNS has had a chance to resolve the devices
            start_ping_timer = self.loop.call_later(
                MDNS_BOOTSTRAP_TIME, self._async_start_ping_status, ping_status
            )
        else:
            # If mDNS is not available, start the ping status immediately
            self._async_start_ping_status(ping_status)

        if settings.status_use_mqtt:
            from .status.mqtt import MqttStatusThread

            status_thread_mqtt = MqttStatusThread(self)
            status_thread_mqtt.start()

        try:
            await asyncio.Event().wait()
        finally:
            _LOGGER.info("Shutting down...")
            self.stop_event.set()
            self.ping_request.set()
            if start_ping_timer:
                start_ping_timer.cancel()
            if self._ping_status_task:
                self._ping_status_task.cancel()
                self._ping_status_task = None
            if mdns_task:
                mdns_task.cancel()
            if settings.status_use_mqtt:
                status_thread_mqtt.join()
                self.mqtt_ping_request.set()
            for task in self._background_tasks:
                task.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await task
            await asyncio.sleep(0)

    def async_create_background_task(
        self, coro: Coroutine[Any, Any, Any]
    ) -> asyncio.Task:
        """Create a background task."""
        task = self.loop.create_task(coro)
        task.add_done_callback(self._background_tasks.discard)
        return task


DASHBOARD = ESPHomeDashboard()
