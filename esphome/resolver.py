"""DNS resolver for ESPHome using aioesphomeapi."""

from __future__ import annotations

import asyncio
import threading

from aioesphomeapi.core import ResolveAPIError, ResolveTimeoutAPIError
import aioesphomeapi.host_resolver as hr

from esphome.core import EsphomeError

RESOLVE_TIMEOUT = 10.0  # seconds


class AsyncResolver:
    """Resolver using aioesphomeapi that runs in a thread for faster results.

    This resolver uses aioesphomeapi's async_resolve_host to handle DNS resolution,
    including proper .local domain fallback. Running in a thread allows us to get
    the result immediately without waiting for asyncio.run() to complete its
    cleanup cycle, which can take significant time.
    """

    def __init__(self) -> None:
        """Initialize the resolver."""
        self.result: list[hr.AddrInfo] | None = None
        self.exception: Exception | None = None
        self.event = threading.Event()

    async def _resolve(self, hosts: list[str], port: int) -> None:
        """Resolve hostnames to IP addresses."""
        try:
            self.result = await hr.async_resolve_host(
                hosts, port, timeout=RESOLVE_TIMEOUT
            )
        except (ResolveAPIError, ResolveTimeoutAPIError, OSError) as e:
            self.exception = e
        finally:
            self.event.set()

    def run(self, hosts: list[str], port: int) -> list[hr.AddrInfo]:
        """Run the DNS resolution in a separate thread."""
        thread = threading.Thread(
            target=lambda: asyncio.run(self._resolve(hosts, port)), daemon=True
        )
        thread.start()

        if not self.event.wait(
            timeout=RESOLVE_TIMEOUT + 1.0
        ):  # Give it 1 second more than the resolver timeout
            raise EsphomeError("Timeout resolving IP address")

        if exc := self.exception:
            if isinstance(exc, ResolveTimeoutAPIError):
                raise EsphomeError(f"Timeout resolving IP address: {exc}") from exc
            if isinstance(exc, ResolveAPIError):
                raise EsphomeError(f"Error resolving IP address: {exc}") from exc
            raise exc

        return self.result
