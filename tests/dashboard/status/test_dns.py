"""Unit tests for esphome.dashboard.dns module."""

from __future__ import annotations

import time
from unittest.mock import patch

import pytest

from esphome.dashboard.dns import DNSCache


@pytest.fixture
def dns_cache_fixture() -> DNSCache:
    """Create a DNSCache instance."""
    return DNSCache()


def test_get_cached_addresses_not_in_cache(dns_cache_fixture: DNSCache) -> None:
    """Test get_cached_addresses when hostname is not in cache."""
    now = time.monotonic()
    result = dns_cache_fixture.get_cached_addresses("unknown.example.com", now)
    assert result is None


def test_get_cached_addresses_expired(dns_cache_fixture: DNSCache) -> None:
    """Test get_cached_addresses when cache entry is expired."""
    now = time.monotonic()
    # Add entry that's already expired
    dns_cache_fixture._cache["example.com"] = (now - 1, ["192.168.1.10"])

    result = dns_cache_fixture.get_cached_addresses("example.com", now)
    assert result is None
    # Expired entry should still be in cache (not removed by get_cached_addresses)
    assert "example.com" in dns_cache_fixture._cache


def test_get_cached_addresses_valid(dns_cache_fixture: DNSCache) -> None:
    """Test get_cached_addresses with valid cache entry."""
    now = time.monotonic()
    # Add entry that expires in 60 seconds
    dns_cache_fixture._cache["example.com"] = (
        now + 60,
        ["192.168.1.10", "192.168.1.11"],
    )

    result = dns_cache_fixture.get_cached_addresses("example.com", now)
    assert result == ["192.168.1.10", "192.168.1.11"]
    # Entry should still be in cache
    assert "example.com" in dns_cache_fixture._cache


def test_get_cached_addresses_hostname_normalization(
    dns_cache_fixture: DNSCache,
) -> None:
    """Test get_cached_addresses normalizes hostname."""
    now = time.monotonic()
    # Add entry with lowercase hostname
    dns_cache_fixture._cache["example.com"] = (now + 60, ["192.168.1.10"])

    # Test with various forms
    assert dns_cache_fixture.get_cached_addresses("EXAMPLE.COM", now) == [
        "192.168.1.10"
    ]
    assert dns_cache_fixture.get_cached_addresses("example.com.", now) == [
        "192.168.1.10"
    ]
    assert dns_cache_fixture.get_cached_addresses("EXAMPLE.COM.", now) == [
        "192.168.1.10"
    ]


def test_get_cached_addresses_ipv6(dns_cache_fixture: DNSCache) -> None:
    """Test get_cached_addresses with IPv6 addresses."""
    now = time.monotonic()
    dns_cache_fixture._cache["example.com"] = (now + 60, ["2001:db8::1", "fe80::1"])

    result = dns_cache_fixture.get_cached_addresses("example.com", now)
    assert result == ["2001:db8::1", "fe80::1"]


def test_get_cached_addresses_empty_list(dns_cache_fixture: DNSCache) -> None:
    """Test get_cached_addresses with empty address list."""
    now = time.monotonic()
    dns_cache_fixture._cache["example.com"] = (now + 60, [])

    result = dns_cache_fixture.get_cached_addresses("example.com", now)
    assert result == []


def test_get_cached_addresses_exception_in_cache(dns_cache_fixture: DNSCache) -> None:
    """Test get_cached_addresses when cache contains an exception."""
    now = time.monotonic()
    # Store an exception (from failed resolution)
    dns_cache_fixture._cache["example.com"] = (now + 60, OSError("Resolution failed"))

    result = dns_cache_fixture.get_cached_addresses("example.com", now)
    assert result is None  # Should return None for exceptions


def test_async_resolve_not_called(dns_cache_fixture: DNSCache) -> None:
    """Test that get_cached_addresses never calls async_resolve."""
    now = time.monotonic()

    with patch.object(dns_cache_fixture, "async_resolve") as mock_resolve:
        # Test non-cached
        result = dns_cache_fixture.get_cached_addresses("uncached.com", now)
        assert result is None
        mock_resolve.assert_not_called()

        # Test expired
        dns_cache_fixture._cache["expired.com"] = (now - 1, ["192.168.1.10"])
        result = dns_cache_fixture.get_cached_addresses("expired.com", now)
        assert result is None
        mock_resolve.assert_not_called()

        # Test valid
        dns_cache_fixture._cache["valid.com"] = (now + 60, ["192.168.1.10"])
        result = dns_cache_fixture.get_cached_addresses("valid.com", now)
        assert result == ["192.168.1.10"]
        mock_resolve.assert_not_called()
