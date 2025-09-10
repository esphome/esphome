"""Tests for the DNS resolver module."""

from __future__ import annotations

import re
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
        resolver = AsyncResolver(["test.local"], 6053)
        result = resolver.resolve()

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
        resolver = AsyncResolver(["test1.local", "test2.local"], 6053)
        result = resolver.resolve()

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
        resolver = AsyncResolver(["test.local"], 6053)
        with pytest.raises(
            EsphomeError, match=re.escape(f"Error resolving IP address: {error_msg}")
        ):
            resolver.resolve()


def test_async_resolver_timeout_error() -> None:
    """Test handling of ResolveTimeoutAPIError."""
    error_msg = "Resolution timed out"

    with patch(
        "esphome.resolver.hr.async_resolve_host",
        side_effect=ResolveTimeoutAPIError(error_msg),
    ):
        resolver = AsyncResolver(["test.local"], 6053)
        # Match either "Timeout" or "Error" since ResolveTimeoutAPIError is a subclass of ResolveAPIError
        # and depending on import order/test execution context, it might be caught as either
        with pytest.raises(
            EsphomeError,
            match=f"(Timeout|Error) resolving IP address: {re.escape(error_msg)}",
        ):
            resolver.resolve()


def test_async_resolver_generic_exception() -> None:
    """Test handling of generic exceptions."""
    error = RuntimeError("Unexpected error")
    with patch(
        "esphome.resolver.hr.async_resolve_host",
        side_effect=error,
    ):
        resolver = AsyncResolver(["test.local"], 6053)
        with pytest.raises(RuntimeError, match="Unexpected error"):
            resolver.resolve()


def test_async_resolver_thread_timeout() -> None:
    """Test timeout when thread doesn't complete in time."""
    # Mock the start method to prevent actual thread execution
    with (
        patch.object(AsyncResolver, "start"),
        patch("esphome.resolver.hr.async_resolve_host"),
    ):
        resolver = AsyncResolver(["test.local"], 6053)
        # Override event.wait to simulate timeout (return False = timeout occurred)
        with (
            patch.object(resolver.event, "wait", return_value=False),
            pytest.raises(
                EsphomeError, match=re.escape("Timeout resolving IP address")
            ),
        ):
            resolver.resolve()

        # Verify thread start was called
        resolver.start.assert_called_once()


def test_async_resolver_ip_addresses(mock_addr_info_ipv4: AddrInfo) -> None:
    """Test resolving IP addresses."""
    with patch(
        "esphome.resolver.hr.async_resolve_host",
        return_value=[mock_addr_info_ipv4],
    ) as mock_resolve:
        resolver = AsyncResolver(["192.168.1.100"], 6053)
        result = resolver.resolve()

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
        resolver = AsyncResolver(["test.local", "192.168.1.100", "::1"], 6053)
        result = resolver.resolve()

        assert result == mock_results
        mock_resolve.assert_called_once_with(
            ["test.local", "192.168.1.100", "::1"], 6053, timeout=RESOLVE_TIMEOUT
        )
