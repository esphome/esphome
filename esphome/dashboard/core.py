from __future__ import annotations

import asyncio
import logging
import threading
from typing import TYPE_CHECKING

from ..zeroconf import DiscoveredImport
from .entries import DashboardEntries
from .settings import DashboardSettings

if TYPE_CHECKING:
    from .status.mdns import MDNSStatus

_LOGGER = logging.getLogger(__name__)


class ESPHomeDashboard:
    """Class that represents the dashboard."""

    __slots__ = (
        "entries",
        "loop",
        "ping_result",
        "import_result",
        "stop_event",
        "ping_request",
        "mqtt_ping_request",
        "mdns_status",
        "settings",
    )

    def __init__(self) -> None:
        """Initialize the ESPHomeDashboard."""
        self.entries: DashboardEntries | None = None
        self.loop: asyncio.AbstractEventLoop | None = None
        self.ping_result: dict[str, bool | None] = {}
        self.import_result: dict[str, DiscoveredImport] = {}
        self.stop_event = threading.Event()
        self.ping_request: asyncio.Event | None = None
        self.mqtt_ping_request = threading.Event()
        self.mdns_status: MDNSStatus | None = None
        self.settings: DashboardSettings = DashboardSettings()

    async def async_setup(self) -> None:
        """Setup the dashboard."""
        self.loop = asyncio.get_running_loop()
        self.ping_request = asyncio.Event()
        self.entries = DashboardEntries(self.settings.config_dir)

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
            await asyncio.sleep(0)


DASHBOARD = ESPHomeDashboard()
