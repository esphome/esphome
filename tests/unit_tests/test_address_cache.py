"""Tests for the address_cache module."""

from esphome.address_cache import AddressCache, normalize_hostname


class TestNormalizeHostname:
    """Test the normalize_hostname function."""

    def test_normalize_simple_hostname(self):
        """Test normalizing a simple hostname."""
        assert normalize_hostname("device") == "device"
        assert normalize_hostname("device.local") == "device.local"
        assert normalize_hostname("server.example.com") == "server.example.com"

    def test_normalize_removes_trailing_dots(self):
        """Test that trailing dots are removed."""
        assert normalize_hostname("device.") == "device"
        assert normalize_hostname("device.local.") == "device.local"
        assert normalize_hostname("server.example.com.") == "server.example.com"
        assert normalize_hostname("device...") == "device"

    def test_normalize_converts_to_lowercase(self):
        """Test that hostnames are converted to lowercase."""
        assert normalize_hostname("DEVICE") == "device"
        assert normalize_hostname("Device.Local") == "device.local"
        assert normalize_hostname("Server.Example.COM") == "server.example.com"

    def test_normalize_combined(self):
        """Test combination of trailing dots and case conversion."""
        assert normalize_hostname("DEVICE.LOCAL.") == "device.local"
        assert normalize_hostname("Server.Example.COM...") == "server.example.com"


class TestAddressCache:
    """Test the AddressCache class."""

    def test_init_empty(self):
        """Test initialization with empty caches."""
        cache = AddressCache()
        assert cache.mdns_cache == {}
        assert cache.dns_cache == {}
        assert not cache.has_cache()

    def test_init_with_caches(self):
        """Test initialization with provided caches."""
        mdns_cache = {"device.local": ["192.168.1.10"]}
        dns_cache = {"server.com": ["10.0.0.1"]}
        cache = AddressCache(mdns_cache=mdns_cache, dns_cache=dns_cache)
        assert cache.mdns_cache == mdns_cache
        assert cache.dns_cache == dns_cache
        assert cache.has_cache()

    def test_get_mdns_addresses(self):
        """Test getting mDNS addresses."""
        cache = AddressCache(
            mdns_cache={"device.local": ["192.168.1.10", "192.168.1.11"]}
        )

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

    def test_get_dns_addresses(self):
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

    def test_get_addresses_auto_detection(self):
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

    def test_has_cache(self):
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

    def test_from_cli_args_empty(self):
        """Test creating cache from empty CLI arguments."""
        cache = AddressCache.from_cli_args([], [])
        assert cache.mdns_cache == {}
        assert cache.dns_cache == {}

    def test_from_cli_args_single_entry(self):
        """Test creating cache from single CLI argument."""
        mdns_args = ["device.local=192.168.1.10"]
        dns_args = ["server.com=10.0.0.1"]

        cache = AddressCache.from_cli_args(mdns_args, dns_args)

        assert cache.mdns_cache == {"device.local": ["192.168.1.10"]}
        assert cache.dns_cache == {"server.com": ["10.0.0.1"]}

    def test_from_cli_args_multiple_ips(self):
        """Test creating cache with multiple IPs per host."""
        mdns_args = ["device.local=192.168.1.10,192.168.1.11"]
        dns_args = ["server.com=10.0.0.1,10.0.0.2,10.0.0.3"]

        cache = AddressCache.from_cli_args(mdns_args, dns_args)

        assert cache.mdns_cache == {"device.local": ["192.168.1.10", "192.168.1.11"]}
        assert cache.dns_cache == {"server.com": ["10.0.0.1", "10.0.0.2", "10.0.0.3"]}

    def test_from_cli_args_multiple_entries(self):
        """Test creating cache with multiple host entries."""
        mdns_args = [
            "device1.local=192.168.1.10",
            "device2.local=192.168.1.20,192.168.1.21",
        ]
        dns_args = ["server1.com=10.0.0.1", "server2.com=10.0.0.2"]

        cache = AddressCache.from_cli_args(mdns_args, dns_args)

        assert cache.mdns_cache == {
            "device1.local": ["192.168.1.10"],
            "device2.local": ["192.168.1.20", "192.168.1.21"],
        }
        assert cache.dns_cache == {
            "server1.com": ["10.0.0.1"],
            "server2.com": ["10.0.0.2"],
        }

    def test_from_cli_args_normalization(self):
        """Test that CLI arguments are normalized."""
        mdns_args = ["Device1.Local.=192.168.1.10", "DEVICE2.LOCAL=192.168.1.20"]
        dns_args = ["Server1.COM.=10.0.0.1", "SERVER2.com=10.0.0.2"]

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

    def test_from_cli_args_whitespace_handling(self):
        """Test that whitespace in IPs is handled."""
        mdns_args = ["device.local= 192.168.1.10 , 192.168.1.11 "]
        dns_args = ["server.com= 10.0.0.1 , 10.0.0.2 "]

        cache = AddressCache.from_cli_args(mdns_args, dns_args)

        assert cache.mdns_cache == {"device.local": ["192.168.1.10", "192.168.1.11"]}
        assert cache.dns_cache == {"server.com": ["10.0.0.1", "10.0.0.2"]}

    def test_from_cli_args_invalid_format(self, caplog):
        """Test handling of invalid argument format."""
        mdns_args = ["invalid_format", "device.local=192.168.1.10"]
        dns_args = ["server.com=10.0.0.1", "also_invalid"]

        cache = AddressCache.from_cli_args(mdns_args, dns_args)

        # Valid entries should still be processed
        assert cache.mdns_cache == {"device.local": ["192.168.1.10"]}
        assert cache.dns_cache == {"server.com": ["10.0.0.1"]}

        # Check that warnings were logged for invalid entries
        assert "Invalid cache format: invalid_format" in caplog.text
        assert "Invalid cache format: also_invalid" in caplog.text

    def test_from_cli_args_ipv6(self):
        """Test handling of IPv6 addresses."""
        mdns_args = ["device.local=fe80::1,2001:db8::1"]
        dns_args = ["server.com=2001:db8::2,::1"]

        cache = AddressCache.from_cli_args(mdns_args, dns_args)

        assert cache.mdns_cache == {"device.local": ["fe80::1", "2001:db8::1"]}
        assert cache.dns_cache == {"server.com": ["2001:db8::2", "::1"]}

    def test_logging_output(self, caplog):
        """Test that appropriate debug logging occurs."""
        import logging

        caplog.set_level(logging.DEBUG)

        cache = AddressCache(
            mdns_cache={"device.local": ["192.168.1.10"]},
            dns_cache={"server.com": ["10.0.0.1"]},
        )

        # Test successful lookups log at debug level
        result = cache.get_mdns_addresses("device.local")
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
