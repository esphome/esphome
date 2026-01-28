"""Tests for esphome.dashboard.status.ping."""

from __future__ import annotations

import asyncio
import threading
from unittest.mock import AsyncMock, Mock, patch

import pytest

from esphome.dashboard.entries import EntryStateSource, ReachableState
from esphome.dashboard.status.ping import PingStatus


@pytest.mark.asyncio
async def test_ping_status_host_entry_without_address_falls_back_to_name() -> None:
    """Host platform entries with address=None should still be pinged via entry.name."""
    stop_event = threading.Event()
    ping_request = asyncio.Event()
    ping_request.set()

    entry_host = Mock()
    entry_host.address = None
    entry_host.name = "host-light"
    entry_host.target_platform = "HOST"
    entry_host.state = Mock(
        reachable=ReachableState.UNKNOWN, source=EntryStateSource.UNKNOWN
    )

    entry_non_host = Mock()
    entry_non_host.address = None
    entry_non_host.name = "esp32-node"
    entry_non_host.target_platform = "ESP32"
    entry_non_host.state = Mock(
        reachable=ReachableState.UNKNOWN, source=EntryStateSource.UNKNOWN
    )

    entries = Mock()
    entries.async_all.return_value = [entry_host, entry_non_host]
    entries.async_set_state_if_source = Mock()
    entries.async_set_state_if_online_or_source = Mock()

    dns_cache = Mock()
    dns_cache.async_resolve = AsyncMock(return_value=["127.0.0.1"])

    dashboard = Mock()
    dashboard.stop_event = stop_event
    dashboard.ping_request = ping_request
    dashboard.entries = entries
    dashboard.dns_cache = dns_cache

    ping_status = PingStatus(dashboard)

    host = Mock()
    host.is_alive = True

    async def async_ping_side_effect(*args, **kwargs):
        stop_event.set()
        return host

    with (
        patch(
            "esphome.dashboard.status.ping._can_use_icmp_lib_with_privilege",
            return_value=False,
        ),
        patch("esphome.dashboard.status.ping.MIN_PING_INTERVAL", 0),
        patch(
            "esphome.dashboard.status.ping.async_ping",
            side_effect=async_ping_side_effect,
        ) as mock_ping,
    ):
        await ping_status.async_run()

    dns_cache.async_resolve.assert_called_once()
    assert dns_cache.async_resolve.call_args.args[0] == "host-light"

    mock_ping.assert_called_once()
    assert mock_ping.call_args.args[0] == "127.0.0.1"

    assert entries.async_set_state_if_online_or_source.call_count == 1
    set_entry, set_state = entries.async_set_state_if_online_or_source.call_args.args
    assert set_entry is entry_host
    assert set_state.source is EntryStateSource.PING
    assert set_state.reachable is ReachableState.ONLINE
