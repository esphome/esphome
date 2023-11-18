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
        # This is the hostnames to path mapping
        self.host_name_to_path: dict[str, str] = {}
        self.path_to_host_name: dict[str, str] = {}
        # This is a set of host names to track (i.e no_mdns = false)
        self.host_name_with_mdns_enabled: set[set] = set()
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
        current_entries = dashboard.entries.async_all()
        host_name_with_mdns_enabled = self.host_name_with_mdns_enabled
        host_mdns_state = self.host_mdns_state
        host_name_to_path = self.host_name_to_path
        path_to_host_name = self.path_to_host_name
        entries = dashboard.entries

        for entry in current_entries:
            name = entry.name
            # If no_mdns is set, remove it from the set
            if entry.no_mdns:
                host_name_with_mdns_enabled.discard(name)
                continue

            # We are tracking this host
            host_name_with_mdns_enabled.add(name)
            path = entry.path

            # If we just adopted/imported this host, we likely
            # already have a state for it, so we should make sure
            # to set it so the dashboard shows it as online
            if (online := host_mdns_state.get(name, SENTINEL)) != SENTINEL:
                entries.async_set_state(entry, bool_to_entry_state(online))

            # Make sure the mapping is up to date
            # so when we get an mdns update we can map it back
            # to the filename
            host_name_to_path[name] = path
            path_to_host_name[path] = name

    async def async_run(self) -> None:
        dashboard = DASHBOARD
        entries = dashboard.entries
        aiozc = AsyncEsphomeZeroconf()
        self.aiozc = aiozc
        host_mdns_state = self.host_mdns_state
        host_name_to_path = self.host_name_to_path
        host_name_with_mdns_enabled = self.host_name_with_mdns_enabled

        def on_update(dat: dict[str, bool | None]) -> None:
            """Update the entry state."""
            for name, result in dat.items():
                host_mdns_state[name] = result
                if name not in host_name_with_mdns_enabled:
                    continue
                if entry := entries.get(host_name_to_path[name]):
                    entries.async_set_state(entry, bool_to_entry_state(result))

        stat = DashboardStatus(on_update)
        imports = DashboardImportDiscovery()
        dashboard.import_result = imports.import_state

        browser = DashboardBrowser(
            aiozc.zeroconf,
            ESPHOME_SERVICE_TYPE,
            [stat.browser_callback, imports.browser_callback],
        )

        while not dashboard.stop_event.is_set():
            await self.async_refresh_hosts()
            await dashboard.ping_request.wait()
            dashboard.ping_request.clear()

        await browser.async_cancel()
        await aiozc.async_close()
        self.aiozc = None
