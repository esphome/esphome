from __future__ import annotations

import asyncio
import logging
import time
import typing
from typing import cast

from icmplib import Host, SocketPermissionError, async_ping

from ..const import MAX_EXECUTOR_WORKERS
from ..entries import (
    DashboardEntry,
    EntryState,
    EntryStateSource,
    ReachableState,
    bool_to_entry_state,
)
from ..util.itertools import chunked

if typing.TYPE_CHECKING:
    from ..core import ESPHomeDashboard


_LOGGER = logging.getLogger(__name__)

GROUP_SIZE = int(MAX_EXECUTOR_WORKERS / 2)

DNS_FAILURE_STATE = EntryState(ReachableState.DNS_FAILURE, EntryStateSource.PING)

MIN_PING_INTERVAL = 5  # ensure we don't ping too often


class PingStatus:
    def __init__(self, dashboard: ESPHomeDashboard) -> None:
        """Initialize the PingStatus class."""
        super().__init__()
        self._loop = asyncio.get_running_loop()
        self.dashboard = dashboard

    async def async_run(self) -> None:
        """Run the ping status."""
        dashboard = self.dashboard
        entries = dashboard.entries
        privileged = await _can_use_icmp_lib_with_privilege()
        if privileged is None:
            _LOGGER.warning("Cannot use icmplib because privileges are insufficient")
            return

        while not dashboard.stop_event.is_set():
            # Only ping if the dashboard is open
            await dashboard.ping_request.wait()
            dashboard.ping_request.clear()
            iteration_start = time.monotonic()
            current_entries = dashboard.entries.async_all()
            to_ping: list[DashboardEntry] = []

            for entry in current_entries:
                if entry.address is None:
                    # No address or we already have a state from another source
                    # so no need to ping
                    continue
                if (
                    entry.state.reachable is ReachableState.ONLINE
                    and entry.state.source
                    not in (EntryStateSource.PING, EntryStateSource.UNKNOWN)
                ):
                    # If we already have a state from another source and
                    # it's online, we don't need to ping
                    continue
                to_ping.append(entry)

            # Resolve DNS for all entries
            entries_with_addresses: dict[DashboardEntry, list[str]] = {}
            for ping_group in chunked(to_ping, GROUP_SIZE):
                ping_group = cast(list[DashboardEntry], ping_group)
                now_monotonic = time.monotonic()
                dns_results = await asyncio.gather(
                    *(
                        dashboard.dns_cache.async_resolve(entry.address, now_monotonic)
                        for entry in ping_group
                    ),
                    return_exceptions=True,
                )

                for entry, result in zip(ping_group, dns_results):
                    if isinstance(result, Exception):
                        # Only update state if its unknown or from ping
                        # so we don't mark it as offline if we have a state
                        # from mDNS or MQTT
                        entries.async_set_state_if_source(entry, DNS_FAILURE_STATE)
                        continue
                    if isinstance(result, BaseException):
                        raise result
                    entries_with_addresses[entry] = result

            # Ping all entries with valid addresses
            for ping_group in chunked(entries_with_addresses.items(), GROUP_SIZE):
                entry_addresses = cast(tuple[DashboardEntry, list[str]], ping_group)

                results = await asyncio.gather(
                    *(
                        async_ping(addresses[0], privileged=privileged)
                        for _, addresses in entry_addresses
                    ),
                    return_exceptions=True,
                )

                for entry_addresses, result in zip(entry_addresses, results):
                    if isinstance(result, Exception):
                        ping_result = False
                    elif isinstance(result, BaseException):
                        raise result
                    else:
                        host: Host = result
                        ping_result = host.is_alive
                    entry: DashboardEntry = entry_addresses[0]
                    # If we can reach it via ping, we always set it
                    # online, however if we can't reach it via ping
                    # we only set it to offline if the state is unknown
                    # or from ping
                    entries.async_set_state_if_online_or_source(
                        entry,
                        bool_to_entry_state(ping_result, EntryStateSource.PING),
                    )

            if not dashboard.stop_event.is_set():
                iteration_duration = time.monotonic() - iteration_start
                if iteration_duration < MIN_PING_INTERVAL:
                    await asyncio.sleep(MIN_PING_INTERVAL - iteration_duration)


async def _can_use_icmp_lib_with_privilege() -> None | bool:
    """Verify we can create a raw socket."""
    try:
        await async_ping("127.0.0.1", count=0, timeout=0, privileged=True)
    except SocketPermissionError:
        try:
            await async_ping("127.0.0.1", count=0, timeout=0, privileged=False)
        except SocketPermissionError:
            _LOGGER.debug(
                "Cannot use icmplib because privileges are insufficient to create the"
                " socket"
            )
            return None

        _LOGGER.debug("Using icmplib in privileged=False mode")
        return False

    _LOGGER.debug("Using icmplib in privileged=True mode")
    return True
