from __future__ import annotations

import asyncio
import os
from typing import cast

from ..core import DASHBOARD, list_dashboard_entries
from ..entries import DashboardEntry
from ..util import chunked


async def _async_ping_host(host: str) -> bool:
    """Ping a host."""
    ping_command = ["ping", "-n" if os.name == "nt" else "-c", "1"]
    process = await asyncio.create_subprocess_exec(
        *ping_command,
        host,
        stdin=asyncio.subprocess.DEVNULL,
        stdout=asyncio.subprocess.DEVNULL,
        stderr=asyncio.subprocess.DEVNULL,
        close_fds=False,
    )
    await process.wait()
    return process.returncode == 0


class PingStatus:
    def __init__(self) -> None:
        """Initialize the PingStatus class."""
        super().__init__()
        self._loop = asyncio.get_running_loop()

    async def async_run(self) -> None:
        """Run the ping status."""
        dashboard = DASHBOARD

        while not dashboard.stop_event.is_set():
            # Only ping if the dashboard is open
            await dashboard.ping_request.wait()
            dashboard.ping_result.clear()
            entries = await self._loop.run_in_executor(None, list_dashboard_entries)
            to_ping: list[DashboardEntry] = [
                entry for entry in entries if entry.address is not None
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
                    dashboard.ping_result[entry.filename] = result
