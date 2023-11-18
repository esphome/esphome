from __future__ import annotations

import asyncio
import os
from typing import cast

from ..core import DASHBOARD
from ..entries import DashboardEntry, bool_to_entry_state
from ..util.itertools import chunked
from ..util.subprocess import async_system_command_status


async def _async_ping_host(host: str) -> bool:
    """Ping a host."""
    return await async_system_command_status(
        ["ping", "-n" if os.name == "nt" else "-c", "1", host]
    )


class PingStatus:
    def __init__(self) -> None:
        """Initialize the PingStatus class."""
        super().__init__()
        self._loop = asyncio.get_running_loop()

    async def async_run(self) -> None:
        """Run the ping status."""
        dashboard = DASHBOARD
        entries = dashboard.entries

        while not dashboard.stop_event.is_set():
            # Only ping if the dashboard is open
            await dashboard.ping_request.wait()
            current_entries = dashboard.entries.async_all()
            to_ping: list[DashboardEntry] = [
                entry for entry in current_entries if entry.address is not None
            ]
            for ping_group in chunked(to_ping, 16):
                ping_group = cast(list[DashboardEntry], ping_group)
                results = await asyncio.gather(
                    *(_async_ping_host(entry.address) for entry in ping_group),
                    return_exceptions=True,
                )
                for entry, result in zip(ping_group, results):
                    if isinstance(result, Exception):
                        result = False
                    elif isinstance(result, BaseException):
                        raise result
                    entries.async_set_state(entry, bool_to_entry_state(result))
