from __future__ import annotations

import asyncio

from esphome.zeroconf import (
    ESPHOME_SERVICE_TYPE,
    AsyncEsphomeZeroconf,
    DashboardBrowser,
    DashboardImportDiscovery,
    DashboardStatus,
)

from ..const import SENTINEL
from ..core import DASHBOARD
from ..entries import bool_to_entry_state


class MDNSStatus:
    """Class that updates the mdns status."""

    def __init__(self) -> None:
        """Initialize the MDNSStatus class."""
        super().__init__()
        self.aiozc: AsyncEsphomeZeroconf | None = None
        # This is the current mdns state for each host (True, False, None)
        self.host_mdns_state: dict[str, bool | None] = {}
        self._loop = asyncio.get_running_loop()

    def get_path_to_host_name(self, path: str) -> str | None:
        """Resolve a path to an address in a thread-safe manner."""
        return self.path_to_host_name.get(path)

    async def async_resolve_host(self, host_name: str) -> str | None:
        """Resolve a host name to an address in a thread-safe manner."""
        if aiozc := self.aiozc:
            return await aiozc.async_resolve_host(host_name)
        return None

    async def async_refresh_hosts(self):
        """Refresh the hosts to track."""
        dashboard = DASHBOARD
        host_mdns_state = self.host_mdns_state
        entries = dashboard.entries
        for entry in entries.async_all():
            if entry.no_mdns:
                continue
            # If we just adopted/imported this host, we likely
            # already have a state for it, so we should make sure
            # to set it so the dashboard shows it as online
            if (online := host_mdns_state.get(entry.name, SENTINEL)) != SENTINEL:
                entries.async_set_state(entry, bool_to_entry_state(online))

    async def async_run(self) -> None:
        dashboard = DASHBOARD
        entries = dashboard.entries
        aiozc = AsyncEsphomeZeroconf()
        self.aiozc = aiozc
        host_mdns_state = self.host_mdns_state

        def on_update(dat: dict[str, bool | None]) -> None:
            """Update the entry state."""
            for name, result in dat.items():
                host_mdns_state[name] = result
                if matching_entries := entries.get_by_name(name):
                    for entry in matching_entries:
                        if not entry.no_mdns:
                            entries.async_set_state(entry, bool_to_entry_state(result))

        stat = DashboardStatus(on_update)
        imports = DashboardImportDiscovery()
        dashboard.import_result = imports.import_state

        browser = DashboardBrowser(
            aiozc.zeroconf,
            ESPHOME_SERVICE_TYPE,
            [stat.browser_callback, imports.browser_callback],
        )

        ping_request = dashboard.ping_request
        while not dashboard.stop_event.is_set():
            await self.async_refresh_hosts()
            await ping_request.wait()
            ping_request.clear()

        await browser.async_cancel()
        await aiozc.async_close()
        self.aiozc = None
