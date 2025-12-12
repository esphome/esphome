from __future__ import annotations

import asyncio
from contextlib import suppress
from ipaddress import ip_address

from icmplib import NameLookupError, async_resolve

RESOLVE_TIMEOUT = 3.0


async def _async_resolve_wrapper(hostname: str) -> list[str] | Exception:
    """Wrap the icmplib async_resolve function."""
    with suppress(ValueError):
        return [str(ip_address(hostname))]
    try:
        async with asyncio.timeout(RESOLVE_TIMEOUT):
            return await async_resolve(hostname)
    except (TimeoutError, NameLookupError, UnicodeError) as ex:
        return ex


class DNSCache:
    """DNS cache for the dashboard."""

    def __init__(self, ttl: int | None = 120) -> None:
        """Initialize the DNSCache."""
        self._cache: dict[str, tuple[float, list[str] | Exception]] = {}
        self._ttl = ttl

    def get_cached_addresses(
        self, hostname: str, now_monotonic: float
    ) -> list[str] | None:
        """Get cached addresses without triggering resolution.

        Returns None if not in cache, list of addresses if found.
        """
        # Normalize hostname for consistent lookups
        normalized = hostname.rstrip(".").lower()
        if expire_time_addresses := self._cache.get(normalized):
            expire_time, addresses = expire_time_addresses
            if expire_time > now_monotonic and not isinstance(addresses, Exception):
                return addresses
        return None

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
