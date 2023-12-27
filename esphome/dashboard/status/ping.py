from __future__ import annotations

import asyncio
import logging
from typing import cast

from icmplib import Host, SocketPermissionError, async_ping

from ..core import DASHBOARD
from ..entries import DashboardEntry, bool_to_entry_state, EntryState
from ..util.itertools import chunked


_LOGGER = logging.getLogger(__name__)


class PingStatus:
    def __init__(self) -> None:
        """Initialize the PingStatus class."""
        super().__init__()
        self._loop = asyncio.get_running_loop()

    async def async_run(self) -> None:
        """Run the ping status."""
        dashboard = DASHBOARD
        entries = dashboard.entries
        privileged = await _can_use_icmp_lib_with_privilege()
        if privileged is None:
            _LOGGER.warning("Cannot use icmplib because privileges are insufficient")
            return

        while not dashboard.stop_event.is_set():
            # Only ping if the dashboard is open
            _LOGGER.debug("Waiting for ping request")
            await dashboard.ping_request.wait()
            _LOGGER.debug("Pinging all entries")
            current_entries = dashboard.entries.async_all()
            to_ping: list[DashboardEntry] = [
                entry for entry in current_entries if entry.address is not None
            ]
            # Try to do 20 at a time
            for ping_group in chunked(to_ping, 20):
                ping_group = cast(list[DashboardEntry], ping_group)
                _LOGGER.debug("Pinging group %s", ping_group)
                dns_results = await asyncio.gather(
                    *(
                        dashboard.dns_cache.async_resolve(entry.address)
                        for entry in ping_group
                    ),
                )
                entries_with_addresses: dict[DashboardEntry, list[str]] = {}
                for entry, result in zip(ping_group, dns_results):
                    if result is None:
                        entries.async_set_state(entry, EntryState.UNKNOWN)
                        continue
                    entries_with_addresses[entry] = result

                _LOGGER.debug("Pinging addresses %s", entries_with_addresses)

                results = await asyncio.gather(
                    *(
                        async_ping(addresses[0], privileged=privileged)
                        for entry, addresses in entries_with_addresses.items()
                    ),
                    return_exceptions=True,
                )
                _LOGGER.debug("Ping results %s", results)

                for entry, result in zip(entries_with_addresses, results):
                    if isinstance(result, Exception):
                        ping_result = False
                    elif isinstance(result, BaseException):
                        raise result
                    else:
                        host: Host = result
                        ping_result = host.is_alive
                    entries.async_set_state(entry, bool_to_entry_state(ping_result))


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
