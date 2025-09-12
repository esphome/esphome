"""Unit tests for esphome.dashboard.status.mdns module."""

from __future__ import annotations

from unittest.mock import Mock, patch

import pytest
import pytest_asyncio
from zeroconf import AddressResolver, IPVersion

from esphome.dashboard.status.mdns import MDNSStatus


@pytest_asyncio.fixture
async def mdns_status(mock_dashboard: Mock) -> MDNSStatus:
    """Create an MDNSStatus instance in async context."""
    # We're in an async context so get_running_loop will work
    return MDNSStatus(mock_dashboard)


@pytest.mark.asyncio
async def test_get_cached_addresses_no_zeroconf(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses when no zeroconf instance is available."""
    mdns_status.aiozc = None
    result = mdns_status.get_cached_addresses("device.local")
    assert result is None


@pytest.mark.asyncio
async def test_get_cached_addresses_not_in_cache(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses when address is not in cache."""
    mdns_status.aiozc = Mock()
    mdns_status.aiozc.zeroconf = Mock()

    with patch("esphome.dashboard.status.mdns.AddressResolver") as mock_resolver:
        mock_info = Mock(spec=AddressResolver)
        mock_info.load_from_cache.return_value = False
        mock_resolver.return_value = mock_info

        result = mdns_status.get_cached_addresses("device.local")
        assert result is None
        mock_info.load_from_cache.assert_called_once_with(mdns_status.aiozc.zeroconf)


@pytest.mark.asyncio
async def test_get_cached_addresses_found_in_cache(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses when address is found in cache."""
    mdns_status.aiozc = Mock()
    mdns_status.aiozc.zeroconf = Mock()

    with patch("esphome.dashboard.status.mdns.AddressResolver") as mock_resolver:
        mock_info = Mock(spec=AddressResolver)
        mock_info.load_from_cache.return_value = True
        mock_info.parsed_scoped_addresses.return_value = ["192.168.1.10", "fe80::1"]
        mock_resolver.return_value = mock_info

        result = mdns_status.get_cached_addresses("device.local")
        assert result == ["192.168.1.10", "fe80::1"]
        mock_info.load_from_cache.assert_called_once_with(mdns_status.aiozc.zeroconf)
        mock_info.parsed_scoped_addresses.assert_called_once_with(IPVersion.All)


@pytest.mark.asyncio
async def test_get_cached_addresses_with_trailing_dot(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses with hostname having trailing dot."""
    mdns_status.aiozc = Mock()
    mdns_status.aiozc.zeroconf = Mock()

    with patch("esphome.dashboard.status.mdns.AddressResolver") as mock_resolver:
        mock_info = Mock(spec=AddressResolver)
        mock_info.load_from_cache.return_value = True
        mock_info.parsed_scoped_addresses.return_value = ["192.168.1.10"]
        mock_resolver.return_value = mock_info

        result = mdns_status.get_cached_addresses("device.local.")
        assert result == ["192.168.1.10"]
        # Should normalize to device.local. for zeroconf
        mock_resolver.assert_called_once_with("device.local.")


@pytest.mark.asyncio
async def test_get_cached_addresses_uppercase_hostname(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses with uppercase hostname."""
    mdns_status.aiozc = Mock()
    mdns_status.aiozc.zeroconf = Mock()

    with patch("esphome.dashboard.status.mdns.AddressResolver") as mock_resolver:
        mock_info = Mock(spec=AddressResolver)
        mock_info.load_from_cache.return_value = True
        mock_info.parsed_scoped_addresses.return_value = ["192.168.1.10"]
        mock_resolver.return_value = mock_info

        result = mdns_status.get_cached_addresses("DEVICE.LOCAL")
        assert result == ["192.168.1.10"]
        # Should normalize to device.local. for zeroconf
        mock_resolver.assert_called_once_with("device.local.")


@pytest.mark.asyncio
async def test_get_cached_addresses_simple_hostname(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses with simple hostname (no domain)."""
    mdns_status.aiozc = Mock()
    mdns_status.aiozc.zeroconf = Mock()

    with patch("esphome.dashboard.status.mdns.AddressResolver") as mock_resolver:
        mock_info = Mock(spec=AddressResolver)
        mock_info.load_from_cache.return_value = True
        mock_info.parsed_scoped_addresses.return_value = ["192.168.1.10"]
        mock_resolver.return_value = mock_info

        result = mdns_status.get_cached_addresses("device")
        assert result == ["192.168.1.10"]
        # Should append .local. for zeroconf
        mock_resolver.assert_called_once_with("device.local.")


@pytest.mark.asyncio
async def test_get_cached_addresses_ipv6_only(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses returning only IPv6 addresses."""
    mdns_status.aiozc = Mock()
    mdns_status.aiozc.zeroconf = Mock()

    with patch("esphome.dashboard.status.mdns.AddressResolver") as mock_resolver:
        mock_info = Mock(spec=AddressResolver)
        mock_info.load_from_cache.return_value = True
        mock_info.parsed_scoped_addresses.return_value = ["fe80::1", "2001:db8::1"]
        mock_resolver.return_value = mock_info

        result = mdns_status.get_cached_addresses("device.local")
        assert result == ["fe80::1", "2001:db8::1"]


@pytest.mark.asyncio
async def test_get_cached_addresses_empty_list(mdns_status: MDNSStatus) -> None:
    """Test get_cached_addresses returning empty list from cache."""
    mdns_status.aiozc = Mock()
    mdns_status.aiozc.zeroconf = Mock()

    with patch("esphome.dashboard.status.mdns.AddressResolver") as mock_resolver:
        mock_info = Mock(spec=AddressResolver)
        mock_info.load_from_cache.return_value = True
        mock_info.parsed_scoped_addresses.return_value = []
        mock_resolver.return_value = mock_info

        result = mdns_status.get_cached_addresses("device.local")
        assert result == []


@pytest.mark.asyncio
async def test_async_setup_success(mock_dashboard: Mock) -> None:
    """Test successful async_setup."""
    mdns_status = MDNSStatus(mock_dashboard)
    with patch("esphome.dashboard.status.mdns.AsyncEsphomeZeroconf") as mock_zc:
        mock_zc.return_value = Mock()
        result = mdns_status.async_setup()
        assert result is True
        assert mdns_status.aiozc is not None


@pytest.mark.asyncio
async def test_async_setup_failure(mock_dashboard: Mock) -> None:
    """Test async_setup with OSError."""
    mdns_status = MDNSStatus(mock_dashboard)
    with patch("esphome.dashboard.status.mdns.AsyncEsphomeZeroconf") as mock_zc:
        mock_zc.side_effect = OSError("Network error")
        result = mdns_status.async_setup()
        assert result is False
        assert mdns_status.aiozc is None
