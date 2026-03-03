"""Tests for the address_cache module."""

from __future__ import annotations

import logging

import pytest
from pytest import LogCaptureFixture

from esphome.address_cache import AddressCache, normalize_hostname


def test_normalize_simple_hostname() -> None:
    """Test normalizing a simple hostname."""
    assert normalize_hostname("device") == "device"
    assert normalize_hostname("device.local") == "device.local"
    assert normalize_hostname("server.example.com") == "server.example.com"


def test_normalize_removes_trailing_dots() -> None:
    """Test that trailing dots are removed."""
    assert normalize_hostname("device.") == "device"
    assert normalize_hostname("device.local.") == "device.local"
    assert normalize_hostname("server.example.com.") == "server.example.com"
    assert normalize_hostname("device...") == "device"


def test_normalize_converts_to_lowercase() -> None:
    """Test that hostnames are converted to lowercase."""
    assert normalize_hostname("DEVICE") == "device"
    assert normalize_hostname("Device.Local") == "device.local"
    assert normalize_hostname("Server.Example.COM") == "server.example.com"


def test_normalize_combined() -> None:
    """Test combination of trailing dots and case conversion."""
    assert normalize_hostname("DEVICE.LOCAL.") == "device.local"
    assert normalize_hostname("Server.Example.COM...") == "server.example.com"


def test_init_empty() -> None:
    """Test initialization with empty caches."""
    cache = AddressCache()
    assert cache.mdns_cache == {}
    assert cache.dns_cache == {}
    assert not cache.has_cache()


def test_init_with_caches() -> None:
    """Test initialization with provided caches."""
    mdns_cache: dict[str, list[str]] = {"device.local": ["192.168.1.10"]}
    dns_cache: dict[str, list[str]] = {"server.com": ["10.0.0.1"]}
    cache = AddressCache(mdns_cache=mdns_cache, dns_cache=dns_cache)
    assert cache.mdns_cache == mdns_cache
    assert cache.dns_cache == dns_cache
    assert cache.has_cache()


def test_get_mdns_addresses() -> None:
    """Test getting mDNS addresses."""
    cache = AddressCache(mdns_cache={"device.local": ["192.168.1.10", "192.168.1.11"]})

    # Direct lookup
    assert cache.get_mdns_addresses("device.local") == [
        "192.168.1.10",
        "192.168.1.11",
    ]

    # Case insensitive lookup
    assert cache.get_mdns_addresses("Device.Local") == [
        "192.168.1.10",
        "192.168.1.11",
    ]

    # With trailing dot
    assert cache.get_mdns_addresses("device.local.") == [
        "192.168.1.10",
        "192.168.1.11",
    ]

    # Not found
    assert cache.get_mdns_addresses("unknown.local") is None


def test_get_dns_addresses() -> None:
    """Test getting DNS addresses."""
    cache = AddressCache(dns_cache={"server.com": ["10.0.0.1", "10.0.0.2"]})

    # Direct lookup
    assert cache.get_dns_addresses("server.com") == ["10.0.0.1", "10.0.0.2"]

    # Case insensitive lookup
    assert cache.get_dns_addresses("Server.COM") == ["10.0.0.1", "10.0.0.2"]

    # With trailing dot
    assert cache.get_dns_addresses("server.com.") == ["10.0.0.1", "10.0.0.2"]

    # Not found
    assert cache.get_dns_addresses("unknown.com") is None


def test_get_addresses_auto_detection() -> None:
    """Test automatic cache selection based on hostname."""
    cache = AddressCache(
        mdns_cache={"device.local": ["192.168.1.10"]},
        dns_cache={"server.com": ["10.0.0.1"]},
    )

    # Should use mDNS cache for .local domains
    assert cache.get_addresses("device.local") == ["192.168.1.10"]
    assert cache.get_addresses("device.local.") == ["192.168.1.10"]
    assert cache.get_addresses("Device.Local") == ["192.168.1.10"]

    # Should use DNS cache for non-.local domains
    assert cache.get_addresses("server.com") == ["10.0.0.1"]
    assert cache.get_addresses("server.com.") == ["10.0.0.1"]
    assert cache.get_addresses("Server.COM") == ["10.0.0.1"]

    # Not found
    assert cache.get_addresses("unknown.local") is None
    assert cache.get_addresses("unknown.com") is None


def test_has_cache() -> None:
    """Test checking if cache has entries."""
    # Empty cache
    cache = AddressCache()
    assert not cache.has_cache()

    # Only mDNS cache
    cache = AddressCache(mdns_cache={"device.local": ["192.168.1.10"]})
    assert cache.has_cache()

    # Only DNS cache
    cache = AddressCache(dns_cache={"server.com": ["10.0.0.1"]})
    assert cache.has_cache()

    # Both caches
    cache = AddressCache(
        mdns_cache={"device.local": ["192.168.1.10"]},
        dns_cache={"server.com": ["10.0.0.1"]},
    )
    assert cache.has_cache()


def test_from_cli_args_empty() -> None:
    """Test creating cache from empty CLI arguments."""
    cache = AddressCache.from_cli_args([], [])
    assert cache.mdns_cache == {}
    assert cache.dns_cache == {}


def test_from_cli_args_single_entry() -> None:
    """Test creating cache from single CLI argument."""
    mdns_args: list[str] = ["device.local=192.168.1.10"]
    dns_args: list[str] = ["server.com=10.0.0.1"]

    cache = AddressCache.from_cli_args(mdns_args, dns_args)

    assert cache.mdns_cache == {"device.local": ["192.168.1.10"]}
    assert cache.dns_cache == {"server.com": ["10.0.0.1"]}


def test_from_cli_args_multiple_ips() -> None:
    """Test creating cache with multiple IPs per host."""
    mdns_args: list[str] = ["device.local=192.168.1.10,192.168.1.11"]
    dns_args: list[str] = ["server.com=10.0.0.1,10.0.0.2,10.0.0.3"]

    cache = AddressCache.from_cli_args(mdns_args, dns_args)

    assert cache.mdns_cache == {"device.local": ["192.168.1.10", "192.168.1.11"]}
    assert cache.dns_cache == {"server.com": ["10.0.0.1", "10.0.0.2", "10.0.0.3"]}


def test_from_cli_args_multiple_entries() -> None:
    """Test creating cache with multiple host entries."""
    mdns_args: list[str] = [
        "device1.local=192.168.1.10",
        "device2.local=192.168.1.20,192.168.1.21",
    ]
    dns_args: list[str] = ["server1.com=10.0.0.1", "server2.com=10.0.0.2"]

    cache = AddressCache.from_cli_args(mdns_args, dns_args)

    assert cache.mdns_cache == {
        "device1.local": ["192.168.1.10"],
        "device2.local": ["192.168.1.20", "192.168.1.21"],
    }
    assert cache.dns_cache == {
        "server1.com": ["10.0.0.1"],
        "server2.com": ["10.0.0.2"],
    }


def test_from_cli_args_normalization() -> None:
    """Test that CLI arguments are normalized."""
    mdns_args: list[str] = ["Device1.Local.=192.168.1.10", "DEVICE2.LOCAL=192.168.1.20"]
    dns_args: list[str] = ["Server1.COM.=10.0.0.1", "SERVER2.com=10.0.0.2"]

    cache = AddressCache.from_cli_args(mdns_args, dns_args)

    # Hostnames should be normalized (lowercase, no trailing dots)
    assert cache.mdns_cache == {
        "device1.local": ["192.168.1.10"],
        "device2.local": ["192.168.1.20"],
    }
    assert cache.dns_cache == {
        "server1.com": ["10.0.0.1"],
        "server2.com": ["10.0.0.2"],
    }


def test_from_cli_args_whitespace_handling() -> None:
    """Test that whitespace in IPs is handled."""
    mdns_args: list[str] = ["device.local= 192.168.1.10 , 192.168.1.11 "]
    dns_args: list[str] = ["server.com= 10.0.0.1 , 10.0.0.2 "]

    cache = AddressCache.from_cli_args(mdns_args, dns_args)

    assert cache.mdns_cache == {"device.local": ["192.168.1.10", "192.168.1.11"]}
    assert cache.dns_cache == {"server.com": ["10.0.0.1", "10.0.0.2"]}


def test_from_cli_args_invalid_format(caplog: LogCaptureFixture) -> None:
    """Test handling of invalid argument format."""
    mdns_args: list[str] = ["invalid_format", "device.local=192.168.1.10"]
    dns_args: list[str] = ["server.com=10.0.0.1", "also_invalid"]

    cache = AddressCache.from_cli_args(mdns_args, dns_args)

    # Valid entries should still be processed
    assert cache.mdns_cache == {"device.local": ["192.168.1.10"]}
    assert cache.dns_cache == {"server.com": ["10.0.0.1"]}

    # Check that warnings were logged for invalid entries
    assert "Invalid cache format: invalid_format" in caplog.text
    assert "Invalid cache format: also_invalid" in caplog.text


def test_from_cli_args_ipv6() -> None:
    """Test handling of IPv6 addresses."""
    mdns_args: list[str] = ["device.local=fe80::1,2001:db8::1"]
    dns_args: list[str] = ["server.com=2001:db8::2,::1"]

    cache = AddressCache.from_cli_args(mdns_args, dns_args)

    assert cache.mdns_cache == {"device.local": ["fe80::1", "2001:db8::1"]}
    assert cache.dns_cache == {"server.com": ["2001:db8::2", "::1"]}


def test_logging_output(caplog: LogCaptureFixture) -> None:
    """Test that appropriate debug logging occurs."""
    caplog.set_level(logging.DEBUG)

    cache = AddressCache(
        mdns_cache={"device.local": ["192.168.1.10"]},
        dns_cache={"server.com": ["10.0.0.1"]},
    )

    # Test successful lookups log at debug level
    result: list[str] | None = cache.get_mdns_addresses("device.local")
    assert result == ["192.168.1.10"]
    assert "Using mDNS cache for device.local" in caplog.text

    caplog.clear()
    result = cache.get_dns_addresses("server.com")
    assert result == ["10.0.0.1"]
    assert "Using DNS cache for server.com" in caplog.text

    # Test that failed lookups don't log
    caplog.clear()
    result = cache.get_mdns_addresses("unknown.local")
    assert result is None
    assert "Using mDNS cache" not in caplog.text


@pytest.mark.parametrize(
    "hostname,expected",
    [
        ("test.local", "test.local"),
        ("Test.Local.", "test.local"),
        ("TEST.LOCAL...", "test.local"),
        ("example.com", "example.com"),
        ("EXAMPLE.COM.", "example.com"),
    ],
)
def test_normalize_hostname_parametrized(hostname: str, expected: str) -> None:
    """Test hostname normalization with various inputs."""
    assert normalize_hostname(hostname) == expected


@pytest.mark.parametrize(
    "mdns_arg,expected",
    [
        ("host=1.2.3.4", {"host": ["1.2.3.4"]}),
        ("Host.Local=1.2.3.4,5.6.7.8", {"host.local": ["1.2.3.4", "5.6.7.8"]}),
        ("HOST.LOCAL.=::1", {"host.local": ["::1"]}),
    ],
)
def test_parse_cache_args_parametrized(
    mdns_arg: str, expected: dict[str, list[str]]
) -> None:
    """Test parsing of cache arguments with various formats."""
    cache = AddressCache.from_cli_args([mdns_arg], [])
    assert cache.mdns_cache == expected
