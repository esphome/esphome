"""Address cache for DNS and mDNS lookups."""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterable

_LOGGER = logging.getLogger(__name__)


def normalize_hostname(hostname: str) -> str:
    """Normalize hostname for cache lookups.

    Removes trailing dots and converts to lowercase.
    """
    return hostname.rstrip(".").lower()


class AddressCache:
    """Cache for DNS and mDNS address lookups.

    This cache stores pre-resolved addresses from command-line arguments
    to avoid slow DNS/mDNS lookups during builds.
    """

    def __init__(
        self,
        mdns_cache: dict[str, list[str]] | None = None,
        dns_cache: dict[str, list[str]] | None = None,
    ) -> None:
        """Initialize the address cache.

        Args:
            mdns_cache: Pre-populated mDNS addresses (hostname -> IPs)
            dns_cache: Pre-populated DNS addresses (hostname -> IPs)
        """
        self.mdns_cache = mdns_cache or {}
        self.dns_cache = dns_cache or {}

    def _get_cached_addresses(
        self, hostname: str, cache: dict[str, list[str]], cache_type: str
    ) -> list[str] | None:
        """Get cached addresses from a specific cache.

        Args:
            hostname: The hostname to look up
            cache: The cache dictionary to check
            cache_type: Type of cache for logging ("mDNS" or "DNS")

        Returns:
            List of IP addresses if found in cache, None otherwise
        """
        normalized = normalize_hostname(hostname)
        if addresses := cache.get(normalized):
            _LOGGER.debug("Using %s cache for %s: %s", cache_type, hostname, addresses)
            return addresses
        return None

    def get_mdns_addresses(self, hostname: str) -> list[str] | None:
        """Get cached mDNS addresses for a hostname.

        Args:
            hostname: The hostname to look up (should end with .local)

        Returns:
            List of IP addresses if found in cache, None otherwise
        """
        return self._get_cached_addresses(hostname, self.mdns_cache, "mDNS")

    def get_dns_addresses(self, hostname: str) -> list[str] | None:
        """Get cached DNS addresses for a hostname.

        Args:
            hostname: The hostname to look up

        Returns:
            List of IP addresses if found in cache, None otherwise
        """
        return self._get_cached_addresses(hostname, self.dns_cache, "DNS")

    def get_addresses(self, hostname: str) -> list[str] | None:
        """Get cached addresses for a hostname.

        Checks mDNS cache for .local domains, DNS cache otherwise.

        Args:
            hostname: The hostname to look up

        Returns:
            List of IP addresses if found in cache, None otherwise
        """
        normalized = normalize_hostname(hostname)
        if normalized.endswith(".local"):
            return self.get_mdns_addresses(hostname)
        return self.get_dns_addresses(hostname)

    def has_cache(self) -> bool:
        """Check if any cache entries exist."""
        return bool(self.mdns_cache or self.dns_cache)

    @classmethod
    def from_cli_args(
        cls, mdns_args: Iterable[str], dns_args: Iterable[str]
    ) -> AddressCache:
        """Create cache from command-line arguments.

        Args:
            mdns_args: List of mDNS cache entries like ['host=ip1,ip2']
            dns_args: List of DNS cache entries like ['host=ip1,ip2']

        Returns:
            Configured AddressCache instance
        """
        mdns_cache = cls._parse_cache_args(mdns_args)
        dns_cache = cls._parse_cache_args(dns_args)
        return cls(mdns_cache=mdns_cache, dns_cache=dns_cache)

    @staticmethod
    def _parse_cache_args(cache_args: Iterable[str]) -> dict[str, list[str]]:
        """Parse cache arguments into a dictionary.

        Args:
            cache_args: List of cache mappings like ['host1=ip1,ip2', 'host2=ip3']

        Returns:
            Dictionary mapping normalized hostnames to list of IP addresses
        """
        cache: dict[str, list[str]] = {}
        for arg in cache_args:
            if "=" not in arg:
                _LOGGER.warning(
                    "Invalid cache format: %s (expected 'hostname=ip1,ip2')", arg
                )
                continue
            hostname, ips = arg.split("=", 1)
            # Normalize hostname for consistent lookups
            normalized = normalize_hostname(hostname)
            cache[normalized] = [ip.strip() for ip in ips.split(",")]
        return cache
