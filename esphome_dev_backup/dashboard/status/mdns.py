from __future__ import annotations

import asyncio
import logging
import typing

from esphome.zeroconf import (
    ESPHOME_SERVICE_TYPE,
    AsyncEsphomeZeroconf,
    DashboardBrowser,
    DashboardImportDiscovery,
    DashboardStatus,
)

from ..const import SENTINEL
from ..entries import DashboardEntry, EntryStateSource, bool_to_entry_state

if typing.TYPE_CHECKING:
    from ..core import ESPHomeDashboard

_LOGGER = logging.getLogger(__name__)


class MDNSStatus:
    """Class that updates the mdns status."""

    def __init__(self, dashboard: ESPHomeDashboard) -> None:
        """Initialize the MDNSStatus class."""
        super().__init__()
        self.aiozc: AsyncEsphomeZeroconf | None = None
        # This is the current mdns state for each host (True, False, None)
        self.host_mdns_state: dict[str, bool | None] = {}
        self._loop = asyncio.get_running_loop()
        self.dashboard = dashboard

    def async_setup(self) -> bool:
        """Set up the MDNSStatus class."""
        try:
            self.aiozc = AsyncEsphomeZeroconf()
        except OSError as e:
            _LOGGER.warning(
                "Failed to initialize zeroconf, will fallback to ping: %s", e
            )
            return False
        return True

    async def async_resolve_host(self, host_name: str) -> list[str] | None:
        """Resolve a host name to an address in a thread-safe manner."""
        if aiozc := self.aiozc:
            return await aiozc.async_resolve_host(host_name)
        return None

    async def async_refresh_hosts(self) -> None:
        """Refresh the hosts to track."""
        dashboard = self.dashboard
        host_mdns_state = self.host_mdns_state
        entries = dashboard.entries
        poll_names: dict[str, set[DashboardEntry]] = {}
        for entry in entries.async_all():
            if entry.no_mdns:
                continue
            # If we just adopted/imported this host, we likely
            # already have a state for it, so we should make sure
            # to set it so the dashboard shows it as online
            if entry.loaded_integrations and "api" not in entry.loaded_integrations:
                # No api available so we have to poll since
                # the device won't respond to a request to ._esphomelib._tcp.local.
                poll_names.setdefault(entry.name, set()).add(entry)
            elif (online := host_mdns_state.get(entry.name, SENTINEL)) != SENTINEL:
                self._async_set_state(entry, online)
        if poll_names and self.aiozc:
            results = await asyncio.gather(
                *(self.aiozc.async_resolve_host(name) for name in poll_names)
            )
            for name, address_list in zip(poll_names, results):
                result = bool(address_list)
                host_mdns_state[name] = result
                for entry in poll_names[name]:
                    self._async_set_state(entry, result)

    def _async_set_state(self, entry: DashboardEntry, result: bool | None) -> None:
        """Set the state of an entry."""
        state = bool_to_entry_state(result, EntryStateSource.MDNS)
        if result:
            # If we can reach it via mDNS, we always set it online
            # since its the fastest source if its working
            self.dashboard.entries.async_set_state(entry, state)
        else:
            # However if we can't reach it via mDNS
            # we only set it to offline if the state is unknown
            # or from mDNS
            self.dashboard.entries.async_set_state_if_source(entry, state)

    async def async_run(self) -> None:
        """Run the mdns status."""
        dashboard = self.dashboard
        entries = dashboard.entries
        host_mdns_state = self.host_mdns_state

        def on_update(dat: dict[str, bool | None]) -> None:
            """Update the entry state."""
            for name, result in dat.items():
                host_mdns_state[name] = result
                if matching_entries := entries.get_by_name(name):
                    for entry in matching_entries:
                        self._async_set_state(entry, result)

        stat = DashboardStatus(on_update)
        imports = DashboardImportDiscovery()
        dashboard.import_result = imports.import_state

        browser = DashboardBrowser(
            self.aiozc.zeroconf,
            ESPHOME_SERVICE_TYPE,
            [stat.browser_callback, imports.browser_callback],
        )

        ping_request = dashboard.ping_request
        while not dashboard.stop_event.is_set():
            await self.async_refresh_hosts()
            await ping_request.wait()
            ping_request.clear()

        await browser.async_cancel()
        await self.aiozc.async_close()
        self.aiozc = None
