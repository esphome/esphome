from __future__ import annotations

import time

from icmplib import NameLookupError, async_resolve


class DNSCache:
    """DNS cache for the dashboard."""

    def __init__(self, ttl: int | None = 120) -> None:
        """Initialize the DNSCache."""
        self._cache: dict[str, tuple[float, list[str] | None]] = {}
        self._ttl = ttl

    async def async_resolve(self, hostname: str) -> list[str] | None:
        """Resolve a hostname to a list of IP address."""
        if expire_time_addresses := self._cache.get(hostname):
            expire_time, addresses = expire_time_addresses
            if expire_time > time.monotonic():
                return addresses

        expires = time.monotonic() + self._ttl
        try:
            addresses = await async_resolve(hostname)
        except NameLookupError:
            self._cache[hostname] = (expires, None)
            return None

        self._cache[hostname] = (expires, addresses)
        return addresses
