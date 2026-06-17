"""DNS resolver for ESPHome using aioesphomeapi."""

from __future__ import annotations

import logging
import os

from aioesphomeapi.core import ResolveAPIError, ResolveTimeoutAPIError
import aioesphomeapi.host_resolver as hr

from esphome.async_thread import AsyncThreadRunner
from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)

_DEFAULT_RESOLVE_TIMEOUT = 20.0
_env_timeout = os.environ.get("ESPHOME_RESOLVE_TIMEOUT", _DEFAULT_RESOLVE_TIMEOUT)
try:
    RESOLVE_TIMEOUT = float(_env_timeout)
except ValueError:
    _LOGGER.warning(
        "ESPHOME_RESOLVE_TIMEOUT=%r is not a valid number; using default %.1fs",
        _env_timeout,
        _DEFAULT_RESOLVE_TIMEOUT,
    )
    RESOLVE_TIMEOUT = _DEFAULT_RESOLVE_TIMEOUT


class AsyncResolver:
    """Resolver using aioesphomeapi that runs in a thread for faster results.

    This resolver uses aioesphomeapi's async_resolve_host to handle DNS
    resolution, including proper .local domain fallback. Running in a thread
    (via :class:`AsyncThreadRunner`) allows us to get the result immediately
    without waiting for ``asyncio.run()`` to complete its cleanup cycle, which
    can take significant time.
    """

    def __init__(self, hosts: list[str], port: int) -> None:
        """Initialize the resolver."""
        self.hosts = hosts
        self.port = port

    async def _resolve(self) -> list[hr.AddrInfo]:
        """Resolve hostnames to IP addresses."""
        return await hr.async_resolve_host(
            self.hosts, self.port, timeout=RESOLVE_TIMEOUT
        )

    def resolve(self) -> list[hr.AddrInfo]:
        """Start the thread and wait for the result."""
        runner: AsyncThreadRunner[list[hr.AddrInfo]] = AsyncThreadRunner(self._resolve)
        runner.start()

        if not runner.event.wait(
            timeout=RESOLVE_TIMEOUT + 1.0
        ):  # Give it 1 second more than the resolver timeout
            raise EsphomeError("Timeout resolving IP address")

        if exc := runner.exception:
            if isinstance(exc, ResolveTimeoutAPIError):
                raise EsphomeError(f"Timeout resolving IP address: {exc}") from exc
            if isinstance(exc, ResolveAPIError):
                raise EsphomeError(f"Error resolving IP address: {exc}") from exc
            raise exc

        assert runner.result is not None  # guaranteed when event set and no exception
        return runner.result
