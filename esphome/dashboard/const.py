from __future__ import annotations

from esphome.enum import StrEnum


class DashboardEvent(StrEnum):
    """Dashboard WebSocket event types."""

    # Server -> Client events (backend sends to frontend)
    ENTRY_ADDED = "entry_added"
    ENTRY_REMOVED = "entry_removed"
    ENTRY_UPDATED = "entry_updated"
    ENTRY_STATE_CHANGED = "entry_state_changed"
    IMPORTABLE_DEVICE_ADDED = "importable_device_added"
    IMPORTABLE_DEVICE_REMOVED = "importable_device_removed"
    INITIAL_STATE = "initial_state"  # Sent on WebSocket connection
    PONG = "pong"  # Response to client ping

    # Client -> Server events (frontend sends to backend)
    PING = "ping"  # WebSocket keepalive from client
    REFRESH = "refresh"  # Force backend to poll for changes


MAX_EXECUTOR_WORKERS = 48


SENTINEL = object()

DASHBOARD_COMMAND = ["esphome", "--dashboard"]
