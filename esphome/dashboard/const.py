from __future__ import annotations

EVENT_ENTRY_ADDED = "entry_added"
EVENT_ENTRY_REMOVED = "entry_removed"
EVENT_ENTRY_UPDATED = "entry_updated"
EVENT_ENTRY_STATE_CHANGED = "entry_state_changed"
MAX_EXECUTOR_WORKERS = 48


SENTINEL = object()

DASHBOARD_COMMAND = ["esphome", "--dashboard"]
