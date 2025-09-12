"""Unit tests for esphome.dashboard.dns module."""

from __future__ import annotations

import time
from unittest.mock import patch

import pytest

from esphome.dashboard.dns import DNSCache


@pytest.fixture
def dns_cache() -> DNSCache:
    """Create a DNSCache instance."""
    return DNSCache()


def test_get_cached_addresses_not_in_cache(dns_cache: DNSCache) -> None:
    """Test get_cached_addresses when hostname is not in cache."""
    now = time.monotonic()
    result = dns_cache.get_cached_addresses("unknown.example.com", now)
    assert result is None


def test_get_cached_addresses_expired(dns_cache: DNSCache) -> None:
    """Test get_cached_addresses when cache entry is expired."""
    now = time.monotonic()
    # Add entry that's already expired
    dns_cache.cache["example.com"] = (["192.168.1.10"], now - 1)

    result = dns_cache.get_cached_addresses("example.com", now)
    assert result is None
    # Expired entry should be removed
    assert "example.com" not in dns_cache.cache


def test_get_cached_addresses_valid(dns_cache: DNSCache) -> None:
    """Test get_cached_addresses with valid cache entry."""
    now = time.monotonic()
    # Add entry that expires in 60 seconds
    dns_cache.cache["example.com"] = (["192.168.1.10", "192.168.1.11"], now + 60)

    result = dns_cache.get_cached_addresses("example.com", now)
    assert result == ["192.168.1.10", "192.168.1.11"]
    # Entry should still be in cache
    assert "example.com" in dns_cache.cache


def test_get_cached_addresses_hostname_normalization(dns_cache: DNSCache) -> None:
    """Test get_cached_addresses normalizes hostname."""
    now = time.monotonic()
    # Add entry with lowercase hostname
    dns_cache.cache["example.com"] = (["192.168.1.10"], now + 60)

    # Test with various forms
    assert dns_cache.get_cached_addresses("EXAMPLE.COM", now) == ["192.168.1.10"]
    assert dns_cache.get_cached_addresses("example.com.", now) == ["192.168.1.10"]
    assert dns_cache.get_cached_addresses("EXAMPLE.COM.", now) == ["192.168.1.10"]


def test_get_cached_addresses_ipv6(dns_cache: DNSCache) -> None:
    """Test get_cached_addresses with IPv6 addresses."""
    now = time.monotonic()
    dns_cache.cache["example.com"] = (["2001:db8::1", "fe80::1"], now + 60)

    result = dns_cache.get_cached_addresses("example.com", now)
    assert result == ["2001:db8::1", "fe80::1"]


def test_get_cached_addresses_empty_list(dns_cache: DNSCache) -> None:
    """Test get_cached_addresses with empty address list."""
    now = time.monotonic()
    dns_cache.cache["example.com"] = ([], now + 60)

    result = dns_cache.get_cached_addresses("example.com", now)
    assert result == []


def test_resolve_addresses_already_cached(dns_cache: DNSCache) -> None:
    """Test resolve_addresses when hostname is already cached."""
    now = time.monotonic()
    dns_cache.cache["example.com"] = (["192.168.1.10"], now + 60)

    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        result = dns_cache.resolve_addresses("example.com", ["example.com"])
        assert result == ["192.168.1.10"]
        # Should not call getaddrinfo for cached entry
        mock_getaddrinfo.assert_not_called()


def test_resolve_addresses_not_cached(dns_cache: DNSCache) -> None:
    """Test resolve_addresses when hostname needs resolution."""
    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        mock_getaddrinfo.return_value = [
            (None, None, None, None, ("192.168.1.10", 0)),
            (None, None, None, None, ("192.168.1.11", 0)),
        ]

        result = dns_cache.resolve_addresses("example.com", ["example.com"])
        assert result == ["192.168.1.10", "192.168.1.11"]
        mock_getaddrinfo.assert_called_once_with("example.com", 0)

        # Should be cached now
        assert "example.com" in dns_cache.cache


def test_resolve_addresses_multiple_hostnames(dns_cache: DNSCache) -> None:
    """Test resolve_addresses with multiple hostnames."""
    now = time.monotonic()
    dns_cache.cache["cached.com"] = (["192.168.1.10"], now + 60)

    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        mock_getaddrinfo.return_value = [
            (None, None, None, None, ("10.0.0.1", 0)),
        ]

        result = dns_cache.resolve_addresses(
            "primary.com", ["cached.com", "primary.com", "fallback.com"]
        )
        # Should return cached result for first match
        assert result == ["192.168.1.10"]
        mock_getaddrinfo.assert_not_called()


def test_resolve_addresses_resolution_error(dns_cache: DNSCache) -> None:
    """Test resolve_addresses when resolution fails."""
    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        mock_getaddrinfo.side_effect = OSError("Name resolution failed")

        result = dns_cache.resolve_addresses("example.com", ["example.com"])
        assert result == []
        # Failed resolution should not be cached
        assert "example.com" not in dns_cache.cache


def test_resolve_addresses_ipv6_resolution(dns_cache: DNSCache) -> None:
    """Test resolve_addresses with IPv6 results."""
    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        mock_getaddrinfo.return_value = [
            (None, None, None, None, ("2001:db8::1", 0, 0, 0)),
            (None, None, None, None, ("fe80::1", 0, 0, 0)),
        ]

        result = dns_cache.resolve_addresses("example.com", ["example.com"])
        assert result == ["2001:db8::1", "fe80::1"]


def test_resolve_addresses_duplicate_removal(dns_cache: DNSCache) -> None:
    """Test resolve_addresses removes duplicate addresses."""
    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        mock_getaddrinfo.return_value = [
            (None, None, None, None, ("192.168.1.10", 0)),
            (None, None, None, None, ("192.168.1.10", 0)),  # Duplicate
            (None, None, None, None, ("192.168.1.11", 0)),
        ]

        result = dns_cache.resolve_addresses("example.com", ["example.com"])
        assert result == ["192.168.1.10", "192.168.1.11"]


def test_resolve_addresses_hostname_normalization(dns_cache: DNSCache) -> None:
    """Test resolve_addresses normalizes hostnames."""
    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        mock_getaddrinfo.return_value = [
            (None, None, None, None, ("192.168.1.10", 0)),
        ]

        # Resolve with uppercase and trailing dot
        result = dns_cache.resolve_addresses("EXAMPLE.COM.", ["EXAMPLE.COM."])
        assert result == ["192.168.1.10"]

        # Should be cached with normalized name
        assert "example.com" in dns_cache.cache

        # Should use cached result for different forms
        result = dns_cache.resolve_addresses("example.com", ["example.com"])
        assert result == ["192.168.1.10"]
        # Only called once due to caching
        mock_getaddrinfo.assert_called_once()


def test_cache_expiration_ttl(dns_cache: DNSCache) -> None:
    """Test that cache entries expire after TTL."""
    with patch("socket.getaddrinfo") as mock_getaddrinfo:
        mock_getaddrinfo.return_value = [
            (None, None, None, None, ("192.168.1.10", 0)),
        ]

        # First resolution
        result = dns_cache.resolve_addresses("example.com", ["example.com"])
        assert result == ["192.168.1.10"]
        assert mock_getaddrinfo.call_count == 1

        # Simulate time passing beyond TTL
        with patch("time.monotonic") as mock_time:
            mock_time.return_value = time.monotonic() + 301  # TTL is 300 seconds

            # Should trigger new resolution
            result = dns_cache.resolve_addresses("example.com", ["example.com"])
            assert result == ["192.168.1.10"]
            assert mock_getaddrinfo.call_count == 2
