"""Unit tests for esphome.dashboard.dns module."""

from __future__ import annotations

import time
from unittest.mock import AsyncMock, patch

from icmplib import NameLookupError
import pytest

from esphome.dashboard.dns import DNSCache, _async_resolve_wrapper


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


@pytest.mark.asyncio
async def test_async_resolve_wrapper_ip_address() -> None:
    """Test _async_resolve_wrapper returns IP address directly."""
    result = await _async_resolve_wrapper("192.168.1.10")
    assert result == ["192.168.1.10"]

    result = await _async_resolve_wrapper("2001:db8::1")
    assert result == ["2001:db8::1"]


@pytest.mark.asyncio
async def test_async_resolve_wrapper_local_fallback_success() -> None:
    """Test _async_resolve_wrapper falls back to bare hostname for .local."""
    mock_resolve = AsyncMock()
    # First call (device.local) fails, second call (device) succeeds
    mock_resolve.side_effect = [
        NameLookupError("device.local"),
        ["192.168.1.50"],
    ]

    with patch("esphome.dashboard.dns.async_resolve", mock_resolve):
        result = await _async_resolve_wrapper("device.local")

    assert result == ["192.168.1.50"]
    assert mock_resolve.call_count == 2
    mock_resolve.assert_any_call("device.local")
    mock_resolve.assert_any_call("device")


@pytest.mark.asyncio
async def test_async_resolve_wrapper_local_fallback_both_fail() -> None:
    """Test _async_resolve_wrapper returns exception when both fail."""
    mock_resolve = AsyncMock()
    original_exception = NameLookupError("device.local")
    mock_resolve.side_effect = [
        original_exception,
        NameLookupError("device"),
    ]

    with patch("esphome.dashboard.dns.async_resolve", mock_resolve):
        result = await _async_resolve_wrapper("device.local")

    # Should return the original exception, not the fallback exception
    assert result is original_exception
    assert mock_resolve.call_count == 2


@pytest.mark.asyncio
async def test_async_resolve_wrapper_non_local_no_fallback() -> None:
    """Test _async_resolve_wrapper doesn't fallback for non-.local hostnames."""
    mock_resolve = AsyncMock()
    original_exception = NameLookupError("device.example.com")
    mock_resolve.side_effect = original_exception

    with patch("esphome.dashboard.dns.async_resolve", mock_resolve):
        result = await _async_resolve_wrapper("device.example.com")

    assert result is original_exception
    # Should only try the original hostname, no fallback
    assert mock_resolve.call_count == 1
    mock_resolve.assert_called_once_with("device.example.com")


@pytest.mark.asyncio
async def test_async_resolve_wrapper_local_success_no_fallback() -> None:
    """Test _async_resolve_wrapper doesn't fallback when .local succeeds."""
    mock_resolve = AsyncMock(return_value=["192.168.1.50"])

    with patch("esphome.dashboard.dns.async_resolve", mock_resolve):
        result = await _async_resolve_wrapper("device.local")

    assert result == ["192.168.1.50"]
    # Should only try once since it succeeded
    assert mock_resolve.call_count == 1
    mock_resolve.assert_called_once_with("device.local")
