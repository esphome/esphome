from __future__ import annotations

import asyncio

from esphome.zeroconf import (
    ESPHOME_SERVICE_TYPE,
    AsyncEsphomeZeroconf,
    DashboardBrowser,
    DashboardImportDiscovery,
    DashboardStatus,
)

from ..core import DASHBOARD


class MDNSStatus:
    """Class that updates the mdns status."""

    def __init__(self) -> None:
        """Initialize the MDNSStatus class."""
        super().__init__()
        self.aiozc: AsyncEsphomeZeroconf | None = None
        # This is the current mdns state for each host (True, False, None)
        self.host_mdns_state: dict[str, bool | None] = {}
        # This is the hostnames to filenames mapping
        self.host_name_to_filename: dict[str, str] = {}
        self.filename_to_host_name: dict[str, str] = {}
        # This is a set of host names to track (i.e no_mdns = false)
        self.host_name_with_mdns_enabled: set[set] = set()
        self._loop = asyncio.get_running_loop()

    def filename_to_host_name_thread_safe(self, filename: str) -> str | None:
        """Resolve a filename to an address in a thread-safe manner."""
        return self.filename_to_host_name.get(filename)

    async def async_resolve_host(self, host_name: str) -> str | None:
        """Resolve a host name to an address in a thread-safe manner."""
        if aiozc := self.aiozc:
            return await aiozc.async_resolve_host(host_name)
        return None

    async def async_refresh_hosts(self):
        """Refresh the hosts to track."""
        dashboard = DASHBOARD
        entries = dashboard.entries.async_all()
        host_name_with_mdns_enabled = self.host_name_with_mdns_enabled
        host_mdns_state = self.host_mdns_state
        host_name_to_filename = self.host_name_to_filename
        filename_to_host_name = self.filename_to_host_name
        ping_result = dashboard.ping_result

        for entry in entries:
            name = entry.name
            # If no_mdns is set, remove it from the set
            if entry.no_mdns:
                host_name_with_mdns_enabled.discard(name)
                continue

            # We are tracking this host
            host_name_with_mdns_enabled.add(name)
            filename = entry.filename

            # If we just adopted/imported this host, we likely
            # already have a state for it, so we should make sure
            # to set it so the dashboard shows it as online
            if name in host_mdns_state:
                ping_result[filename] = host_mdns_state[name]

            # Make sure the mapping is up to date
            # so when we get an mdns update we can map it back
            # to the filename
            host_name_to_filename[name] = filename
            filename_to_host_name[filename] = name

    async def async_run(self) -> None:
        dashboard = DASHBOARD

        aiozc = AsyncEsphomeZeroconf()
        self.aiozc = aiozc
        host_mdns_state = self.host_mdns_state
        host_name_to_filename = self.host_name_to_filename
        host_name_with_mdns_enabled = self.host_name_with_mdns_enabled
        ping_result = dashboard.ping_result

        def on_update(dat: dict[str, bool | None]) -> None:
            """Update the global PING_RESULT dict."""
            for name, result in dat.items():
                host_mdns_state[name] = result
                if name in host_name_with_mdns_enabled:
                    filename = host_name_to_filename[name]
                    ping_result[filename] = result

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
