from __future__ import annotations

from esphome.enum import StrEnum


class DashboardEvent(StrEnum):
    """Dashboard WebSocket event types."""

    # Internal dashboard events
    ENTRY_ADDED = "entry_added"
    ENTRY_REMOVED = "entry_removed"
    ENTRY_UPDATED = "entry_updated"
    ENTRY_STATE_CHANGED = "entry_state_changed"
    IMPORTABLE_DEVICE_ADDED = "importable_device_added"
    IMPORTABLE_DEVICE_REMOVED = "importable_device_removed"

    # Connection level events
    INITIAL_STATE = "initial_state"  # Sent on WebSocket connection
    PING = "ping"  # WebSocket keepalive
    PONG = "pong"  # WebSocket keepalive response


MAX_EXECUTOR_WORKERS = 48


SENTINEL = object()

DASHBOARD_COMMAND = ["esphome", "--dashboard"]
