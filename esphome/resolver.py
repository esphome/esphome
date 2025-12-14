"""DNS resolver for ESPHome using aioesphomeapi."""

from __future__ import annotations

import asyncio
import threading

from aioesphomeapi.core import ResolveAPIError, ResolveTimeoutAPIError
import aioesphomeapi.host_resolver as hr

from esphome.core import EsphomeError

RESOLVE_TIMEOUT = 10.0  # seconds


class AsyncResolver(threading.Thread):
    """Resolver using aioesphomeapi that runs in a thread for faster results.

    This resolver uses aioesphomeapi's async_resolve_host to handle DNS resolution,
    including proper .local domain fallback. Running in a thread allows us to get
    the result immediately without waiting for asyncio.run() to complete its
    cleanup cycle, which can take significant time.
    """

    def __init__(self, hosts: list[str], port: int) -> None:
        """Initialize the resolver."""
        super().__init__(daemon=True)
        self.hosts = hosts
        self.port = port
        self.result: list[hr.AddrInfo] | None = None
        self.exception: Exception | None = None
        self.event = threading.Event()

    async def _resolve(self) -> None:
        """Resolve hostnames to IP addresses."""
        try:
            self.result = await hr.async_resolve_host(
                self.hosts, self.port, timeout=RESOLVE_TIMEOUT
            )
        except Exception as e:  # pylint: disable=broad-except
            # We need to catch all exceptions to ensure the event is set
            # Otherwise the thread could hang forever
            self.exception = e
        finally:
            self.event.set()

    def run(self) -> None:
        """Run the DNS resolution."""
        asyncio.run(self._resolve())

    def resolve(self) -> list[hr.AddrInfo]:
        """Start the thread and wait for the result."""
        self.start()

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
