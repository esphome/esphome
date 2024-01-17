from __future__ import annotations

import asyncio
import logging
import time
from typing import cast

from icmplib import Host, SocketPermissionError, async_ping

from ..const import MAX_EXECUTOR_WORKERS
from ..core import DASHBOARD
from ..entries import DashboardEntry, EntryState, bool_to_entry_state
from ..util.itertools import chunked

_LOGGER = logging.getLogger(__name__)

GROUP_SIZE = int(MAX_EXECUTOR_WORKERS / 2)


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
            await dashboard.ping_request.wait()
            dashboard.ping_request.clear()
            current_entries = dashboard.entries.async_all()
            to_ping: list[DashboardEntry] = [
                entry for entry in current_entries if entry.address is not None
            ]

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
                        entries.async_set_state(entry, EntryState.UNKNOWN)
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
                    entry, _ = entry_addresses
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
