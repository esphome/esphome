from __future__ import annotations

import asyncio
import logging
import threading

from ..zeroconf import DiscoveredImport
from .settings import DashboardSettings
from .status.mdns import MDNSStatus
from .status.mqtt import MqttStatusThread
from .status.ping import PingStatus
from .entries import DashboardEntry

_LOGGER = logging.getLogger(__name__)


def list_dashboard_entries() -> list[DashboardEntry]:
    """List all dashboard entries."""
    return DASHBOARD.settings.entries()


class ESPHomeDashboard:
    """Class that represents the dashboard."""

    def __init__(self) -> None:
        """Initialize the ESPHomeDashboard."""
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
        self._ping_request = asyncio.Event()

    async def async_run(self) -> None:
        """Run the dashboard."""
        settings = self.settings
        mdns_task: asyncio.Task | None = None
        ping_status_task: asyncio.Task | None = None

        if settings.status_use_ping:
            ping_status = PingStatus()
            ping_status_task = asyncio.create_task(ping_status.async_run())
        else:
            mdns_status = MDNSStatus()
            await mdns_status.async_refresh_hosts()
            self.mdns_status = mdns_status
            mdns_task = asyncio.create_task(mdns_status.async_run())

        if settings.status_use_mqtt:
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
