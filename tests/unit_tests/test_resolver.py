"""Tests for the DNS resolver module."""

from __future__ import annotations

import asyncio
import socket
from unittest.mock import patch

from aioesphomeapi.core import ResolveAPIError, ResolveTimeoutAPIError
from aioesphomeapi.host_resolver import AddrInfo, IPv4Sockaddr, IPv6Sockaddr
import pytest

from esphome.core import EsphomeError
from esphome.resolver import RESOLVE_TIMEOUT, AsyncResolver


@pytest.fixture
def mock_addr_info_ipv4() -> AddrInfo:
    """Create a mock IPv4 AddrInfo."""
    return AddrInfo(
        family=socket.AF_INET,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv4Sockaddr(address="192.168.1.100", port=6053),
    )


@pytest.fixture
def mock_addr_info_ipv6() -> AddrInfo:
    """Create a mock IPv6 AddrInfo."""
    return AddrInfo(
        family=socket.AF_INET6,
        type=socket.SOCK_STREAM,
        proto=socket.IPPROTO_TCP,
        sockaddr=IPv6Sockaddr(address="2001:db8::1", port=6053, flowinfo=0, scope_id=0),
    )


def test_async_resolver_successful_resolution(mock_addr_info_ipv4: AddrInfo) -> None:
    """Test successful DNS resolution."""
    with patch(
        "esphome.resolver.hr.async_resolve_host",
        return_value=[mock_addr_info_ipv4],
    ) as mock_resolve:
        resolver = AsyncResolver()
        result = resolver.run(["test.local"], 6053)

        assert result == [mock_addr_info_ipv4]
        mock_resolve.assert_called_once_with(
            ["test.local"], 6053, timeout=RESOLVE_TIMEOUT
        )


def test_async_resolver_multiple_hosts(
    mock_addr_info_ipv4: AddrInfo, mock_addr_info_ipv6: AddrInfo
) -> None:
    """Test resolving multiple hosts."""
    mock_results = [mock_addr_info_ipv4, mock_addr_info_ipv6]

    with patch(
        "esphome.resolver.hr.async_resolve_host",
        return_value=mock_results,
    ) as mock_resolve:
        resolver = AsyncResolver()
        result = resolver.run(["test1.local", "test2.local"], 6053)

        assert result == mock_results
        mock_resolve.assert_called_once_with(
            ["test1.local", "test2.local"], 6053, timeout=RESOLVE_TIMEOUT
        )


def test_async_resolver_resolve_api_error() -> None:
    """Test handling of ResolveAPIError."""
    error_msg = "Failed to resolve"
    with patch(
        "esphome.resolver.hr.async_resolve_host",
        side_effect=ResolveAPIError(error_msg),
    ):
        resolver = AsyncResolver()
        with pytest.raises(
            EsphomeError, match=f"Error resolving IP address: {error_msg}"
        ):
            resolver.run(["test.local"], 6053)


def test_async_resolver_timeout_error() -> None:
    """Test handling of ResolveTimeoutAPIError."""
    error_msg = "Resolution timed out"
    with patch(
        "esphome.resolver.hr.async_resolve_host",
        side_effect=ResolveTimeoutAPIError(error_msg),
    ):
        resolver = AsyncResolver()
        with pytest.raises(
            EsphomeError, match=f"Timeout resolving IP address: {error_msg}"
        ):
            resolver.run(["test.local"], 6053)


def test_async_resolver_generic_exception() -> None:
    """Test handling of generic exceptions."""
    error = RuntimeError("Unexpected error")
    with patch(
        "esphome.resolver.hr.async_resolve_host",
        side_effect=error,
    ):
        resolver = AsyncResolver()
        with pytest.raises(RuntimeError, match="Unexpected error"):
            resolver.run(["test.local"], 6053)


def test_async_resolver_thread_timeout() -> None:
    """Test timeout when thread doesn't complete in time."""

    async def slow_resolve(hosts, port, timeout):
        await asyncio.sleep(100)  # Sleep longer than timeout
        return []

    with patch("esphome.resolver.hr.async_resolve_host", slow_resolve):
        resolver = AsyncResolver()
        # Override event.wait to simulate timeout
        with (
            patch.object(resolver.event, "wait", return_value=False),
            pytest.raises(EsphomeError, match="Timeout resolving IP address"),
        ):
            resolver.run(["test.local"], 6053)


def test_async_resolver_ip_addresses(mock_addr_info_ipv4: AddrInfo) -> None:
    """Test resolving IP addresses."""
    with patch(
        "esphome.resolver.hr.async_resolve_host",
        return_value=[mock_addr_info_ipv4],
    ) as mock_resolve:
        resolver = AsyncResolver()
        result = resolver.run(["192.168.1.100"], 6053)

        assert result == [mock_addr_info_ipv4]
        mock_resolve.assert_called_once_with(
            ["192.168.1.100"], 6053, timeout=RESOLVE_TIMEOUT
        )


def test_async_resolver_mixed_addresses(
    mock_addr_info_ipv4: AddrInfo, mock_addr_info_ipv6: AddrInfo
) -> None:
    """Test resolving mix of hostnames and IP addresses."""
    mock_results = [mock_addr_info_ipv4, mock_addr_info_ipv6]

    with patch(
        "esphome.resolver.hr.async_resolve_host",
        return_value=mock_results,
    ) as mock_resolve:
        resolver = AsyncResolver()
        result = resolver.run(["test.local", "192.168.1.100", "::1"], 6053)

        assert result == mock_results
        mock_resolve.assert_called_once_with(
            ["test.local", "192.168.1.100", "::1"], 6053, timeout=RESOLVE_TIMEOUT
        )
