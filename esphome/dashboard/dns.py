from __future__ import annotations

import asyncio
import sys

from icmplib import NameLookupError, async_resolve

if sys.version_info >= (3, 11):
    from asyncio import timeout as async_timeout
else:
    from async_timeout import timeout as async_timeout


async def _async_resolve_wrapper(hostname: str) -> list[str] | Exception:
    """Wrap the icmplib async_resolve function."""
    try:
        async with async_timeout(2):
            return await async_resolve(hostname)
    except (asyncio.TimeoutError, NameLookupError, UnicodeError) as ex:
        return ex


class DNSCache:
    """DNS cache for the dashboard."""

    def __init__(self, ttl: int | None = 120) -> None:
        """Initialize the DNSCache."""
        self._cache: dict[str, tuple[float, list[str] | Exception]] = {}
        self._ttl = ttl

    async def async_resolve(
        self, hostname: str, now_monotonic: float
    ) -> list[str] | Exception:
        """Resolve a hostname to a list of IP address."""
        if expire_time_addresses := self._cache.get(hostname):
            expire_time, addresses = expire_time_addresses
            if expire_time > now_monotonic:
                return addresses

        expires = now_monotonic + self._ttl
        addresses = await _async_resolve_wrapper(hostname)
        self._cache[hostname] = (expires, addresses)
        return addresses
