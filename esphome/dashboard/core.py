from __future__ import annotations

import asyncio
from collections.abc import Coroutine
import contextlib
from dataclasses import dataclass
from functools import partial
import json
import logging
from pathlib import Path
import threading
from typing import TYPE_CHECKING, Any, Callable

from esphome.storage_json import ignored_devices_storage_path

from ..zeroconf import DiscoveredImport
from .dns import DNSCache
from .entries import DashboardEntries
from .settings import DashboardSettings

if TYPE_CHECKING:
    from .status.mdns import MDNSStatus


_LOGGER = logging.getLogger(__name__)

IGNORED_DEVICES_STORAGE_PATH = "ignored-devices.json"


@dataclass
class Event:
    """Dashboard Event."""

    event_type: str
    data: dict[str, Any]


class EventBus:
    """Dashboard event bus."""

    def __init__(self) -> None:
        """Initialize the Dashboard event bus."""
        self._listeners: dict[str, set[Callable[[Event], None]]] = {}

    def async_add_listener(
        self, event_type: str, listener: Callable[[Event], None]
    ) -> Callable[[], None]:
        """Add a listener to the event bus."""
        self._listeners.setdefault(event_type, set()).add(listener)
        return partial(self._async_remove_listener, event_type, listener)

    def _async_remove_listener(
        self, event_type: str, listener: Callable[[Event], None]
    ) -> None:
        """Remove a listener from the event bus."""
        self._listeners[event_type].discard(listener)

    def async_fire(self, event_type: str, event_data: dict[str, Any]) -> None:
        """Fire an event."""
        event = Event(event_type, event_data)

        _LOGGER.debug("Firing event: %s", event)

        for listener in self._listeners.get(event_type, set()):
            listener(event)


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

    async def async_setup(self) -> None:
        """Setup the dashboard."""
        self.loop = asyncio.get_running_loop()
        self.ping_request = asyncio.Event()
        self.entries = DashboardEntries(self)
        self.load_ignored_devices()

    def load_ignored_devices(self) -> None:
        storage_path = Path(ignored_devices_storage_path())
        try:
            with storage_path.open("r", encoding="utf-8") as f_handle:
                data = json.load(f_handle)
                self.ignored_devices = set(data.get("ignored_devices", set()))
        except FileNotFoundError:
            pass

    def save_ignored_devices(self) -> None:
        storage_path = Path(ignored_devices_storage_path())
        with storage_path.open("w", encoding="utf-8") as f_handle:
            json.dump(
                {"ignored_devices": sorted(self.ignored_devices)}, indent=2, fp=f_handle
            )

    async def async_run(self) -> None:
        """Run the dashboard."""
        settings = self.settings
        mdns_task: asyncio.Task | None = None
        ping_status_task: asyncio.Task | None = None
        await self.entries.async_update_entries()

        if settings.status_use_ping:
            from .status.ping import PingStatus

            ping_status = PingStatus()
            ping_status_task = asyncio.create_task(ping_status.async_run())
        else:
            from .status.mdns import MDNSStatus

            mdns_status = MDNSStatus()
            await mdns_status.async_refresh_hosts()
            self.mdns_status = mdns_status
            mdns_task = asyncio.create_task(mdns_status.async_run())

        if settings.status_use_mqtt:
            from .status.mqtt import MqttStatusThread

            status_thread_mqtt = MqttStatusThread()
            status_thread_mqtt.start()

        shutdown_event = asyncio.Event()
        try:
            await shutdown_event.wait()
        finally:
            _LOGGER.info("Shutting down...")
            self.stop_event.set()
            self.ping_request.set()
            if ping_status_task:
                ping_status_task.cancel()
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
