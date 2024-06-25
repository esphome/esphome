from __future__ import annotations

import asyncio
import logging
import os
from collections import defaultdict
from typing import TYPE_CHECKING, Any

from esphome import const, util
from esphome.storage_json import StorageJSON, ext_storage_path

from .const import (
    DASHBOARD_COMMAND,
    EVENT_ENTRY_ADDED,
    EVENT_ENTRY_REMOVED,
    EVENT_ENTRY_STATE_CHANGED,
    EVENT_ENTRY_UPDATED,
)
from .enum import StrEnum
from .util.subprocess import async_run_system_command

if TYPE_CHECKING:
    from .core import ESPHomeDashboard

_LOGGER = logging.getLogger(__name__)


DashboardCacheKeyType = tuple[int, int, float, int]

# Currently EntryState is a simple
# online/offline/unknown enum, but in the future
# it may be expanded to include more states


class EntryState(StrEnum):
    ONLINE = "online"
    OFFLINE = "offline"
    UNKNOWN = "unknown"


_BOOL_TO_ENTRY_STATE = {
    True: EntryState.ONLINE,
    False: EntryState.OFFLINE,
    None: EntryState.UNKNOWN,
}
_ENTRY_STATE_TO_BOOL = {
    EntryState.ONLINE: True,
    EntryState.OFFLINE: False,
    EntryState.UNKNOWN: None,
}


def bool_to_entry_state(value: bool) -> EntryState:
    """Convert a bool to an entry state."""
    return _BOOL_TO_ENTRY_STATE[value]


def entry_state_to_bool(value: EntryState) -> bool | None:
    """Convert an entry state to a bool."""
    return _ENTRY_STATE_TO_BOOL[value]


class DashboardEntries:
    """Represents all dashboard entries."""

    __slots__ = (
        "_dashboard",
        "_loop",
        "_config_dir",
        "_entries",
        "_entry_states",
        "_loaded_entries",
        "_update_lock",
        "_name_to_entry",
    )

    def __init__(self, dashboard: ESPHomeDashboard) -> None:
        """Initialize the DashboardEntries."""
        self._dashboard = dashboard
        self._loop = asyncio.get_running_loop()
        self._config_dir = dashboard.settings.config_dir
        # Entries are stored as
        # {
        #   "path/to/file.yaml": DashboardEntry,
        #   ...
        # }
        self._entries: dict[str, DashboardEntry] = {}
        self._loaded_entries = False
        self._update_lock = asyncio.Lock()
        self._name_to_entry: dict[str, set[DashboardEntry]] = defaultdict(set)

    def get(self, path: str) -> DashboardEntry | None:
        """Get an entry by path."""
        return self._entries.get(path)

    def get_by_name(self, name: str) -> set[DashboardEntry] | None:
        """Get an entry by name."""
        return self._name_to_entry.get(name)

    async def _async_all(self) -> list[DashboardEntry]:
        """Return all entries."""
        return list(self._entries.values())

    def all(self) -> list[DashboardEntry]:
        """Return all entries."""
        return asyncio.run_coroutine_threadsafe(self._async_all(), self._loop).result()

    def async_all(self) -> list[DashboardEntry]:
        """Return all entries."""
        return list(self._entries.values())

    def set_state(self, entry: DashboardEntry, state: EntryState) -> None:
        """Set the state for an entry."""
        asyncio.run_coroutine_threadsafe(
            self._async_set_state(entry, state), self._loop
        ).result()

    async def _async_set_state(self, entry: DashboardEntry, state: EntryState) -> None:
        """Set the state for an entry."""
        self.async_set_state(entry, state)

    def async_set_state(self, entry: DashboardEntry, state: EntryState) -> None:
        """Set the state for an entry."""
        if entry.state == state:
            return
        entry.state = state
        self._dashboard.bus.async_fire(
            EVENT_ENTRY_STATE_CHANGED, {"entry": entry, "state": state}
        )

    async def async_request_update_entries(self) -> None:
        """Request an update of the dashboard entries from disk.

        If an update is already in progress, this will do nothing.
        """
        if self._update_lock.locked():
            _LOGGER.debug("Dashboard entries are already being updated")
            return
        await self.async_update_entries()

    async def async_update_entries(self) -> None:
        """Update the dashboard entries from disk."""
        async with self._update_lock:
            await self._async_update_entries()

    def _load_entries(
        self, entries: dict[DashboardEntry, DashboardCacheKeyType]
    ) -> None:
        """Load all entries from disk."""
        for entry, cache_key in entries.items():
            _LOGGER.debug(
                "Loading dashboard entry %s because cache key changed: %s",
                entry.path,
                cache_key,
            )
            entry.load_from_disk(cache_key)

    async def _async_update_entries(self) -> list[DashboardEntry]:
        """Sync the dashboard entries from disk."""
        _LOGGER.debug("Updating dashboard entries")
        # At some point it would be nice to use watchdog to avoid polling

        path_to_cache_key = await self._loop.run_in_executor(
            None, self._get_path_to_cache_key
        )
        entries = self._entries
        name_to_entry = self._name_to_entry
        added: dict[DashboardEntry, DashboardCacheKeyType] = {}
        updated: dict[DashboardEntry, DashboardCacheKeyType] = {}
        removed: set[DashboardEntry] = {
            entry
            for filename, entry in entries.items()
            if filename not in path_to_cache_key
        }
        original_names: dict[DashboardEntry, str] = {}

        for path, cache_key in path_to_cache_key.items():
            if not (entry := entries.get(path)):
                entry = DashboardEntry(path, cache_key)
                added[entry] = cache_key
                continue

            if entry.cache_key != cache_key:
                updated[entry] = cache_key
                original_names[entry] = entry.name

        if added or updated:
            await self._loop.run_in_executor(
                None, self._load_entries, {**added, **updated}
            )

        bus = self._dashboard.bus
        for entry in added:
            entries[entry.path] = entry
            name_to_entry[entry.name].add(entry)
            bus.async_fire(EVENT_ENTRY_ADDED, {"entry": entry})

        for entry in removed:
            del entries[entry.path]
            name_to_entry[entry.name].discard(entry)
            bus.async_fire(EVENT_ENTRY_REMOVED, {"entry": entry})

        for entry in updated:
            if (original_name := original_names[entry]) != (current_name := entry.name):
                name_to_entry[original_name].discard(entry)
                name_to_entry[current_name].add(entry)
            bus.async_fire(EVENT_ENTRY_UPDATED, {"entry": entry})

    def _get_path_to_cache_key(self) -> dict[str, DashboardCacheKeyType]:
        """Return a dict of path to cache key."""
        path_to_cache_key: dict[str, DashboardCacheKeyType] = {}
        #
        # The cache key is (inode, device, mtime, size)
        # which allows us to avoid locking since it ensures
        # every iteration of this call will always return the newest
        # items from disk at the cost of a stat() call on each
        # file which is much faster than reading the file
        # for the cache hit case which is the common case.
        #
        for file in util.list_yaml_files([self._config_dir]):
            try:
                # Prefer the json storage path if it exists
                stat = os.stat(ext_storage_path(os.path.basename(file)))
            except OSError:
                try:
                    # Fallback to the yaml file if the storage
                    # file does not exist or could not be generated
                    stat = os.stat(file)
                except OSError:
                    # File was deleted, ignore
                    continue
            path_to_cache_key[file] = (
                stat.st_ino,
                stat.st_dev,
                stat.st_mtime,
                stat.st_size,
            )
        return path_to_cache_key

    def async_schedule_storage_json_update(self, filename: str) -> None:
        """Schedule a task to update the storage JSON file."""
        self._dashboard.async_create_background_task(
            async_run_system_command(
                [*DASHBOARD_COMMAND, "compile", "--only-generate", filename]
            )
        )


class DashboardEntry:
    """Represents a single dashboard entry.

    This class is thread-safe and read-only.
    """

    __slots__ = (
        "path",
        "filename",
        "_storage_path",
        "cache_key",
        "storage",
        "state",
        "_to_dict",
    )

    def __init__(self, path: str, cache_key: DashboardCacheKeyType) -> None:
        """Initialize the DashboardEntry."""
        self.path = path
        self.filename: str = os.path.basename(path)
        self._storage_path = ext_storage_path(self.filename)
        self.cache_key = cache_key
        self.storage: StorageJSON | None = None
        self.state = EntryState.UNKNOWN
        self._to_dict: dict[str, Any] | None = None

    def __repr__(self) -> str:
        """Return the representation of this entry."""
        return (
            f"DashboardEntry(path={self.path} "
            f"address={self.address} "
            f"web_port={self.web_port} "
            f"name={self.name} "
            f"no_mdns={self.no_mdns} "
            f"state={self.state} "
            ")"
        )

    def to_dict(self) -> dict[str, Any]:
        """Return a dict representation of this entry.

        The dict includes the loaded configuration but not
        the current state of the entry.
        """
        if self._to_dict is None:
            self._to_dict = {
                "name": self.name,
                "friendly_name": self.friendly_name,
                "configuration": self.filename,
                "loaded_integrations": sorted(self.loaded_integrations),
                "deployed_version": self.update_old,
                "current_version": self.update_new,
                "path": self.path,
                "comment": self.comment,
                "address": self.address,
                "web_port": self.web_port,
                "target_platform": self.target_platform,
            }
        return self._to_dict

    def load_from_disk(self, cache_key: DashboardCacheKeyType | None = None) -> None:
        """Load this entry from disk."""
        self.storage = StorageJSON.load(self._storage_path)
        self._to_dict = None
        #
        # Currently StorageJSON.load() will return None if the file does not exist
        #
        # StorageJSON currently does not provide an updated cache key so we use the
        # one that is passed in.
        #
        # The cache key was read from the disk moments ago and may be stale but
        # it does not matter since we are polling anyways, and the next call to
        # async_update_entries() will load it again in the extremely rare case that
        # it changed between the two calls.
        #
        if cache_key:
            self.cache_key = cache_key

    @property
    def address(self) -> str | None:
        """Return the address of this entry."""
        if self.storage is None:
            return None
        return self.storage.address

    @property
    def no_mdns(self) -> bool | None:
        """Return the no_mdns of this entry."""
        if self.storage is None:
            return None
        return self.storage.no_mdns

    @property
    def web_port(self) -> int | None:
        """Return the web port of this entry."""
        if self.storage is None:
            return None
        return self.storage.web_port

    @property
    def name(self) -> str:
        """Return the name of this entry."""
        if self.storage is None:
            return self.filename.replace(".yml", "").replace(".yaml", "")
        return self.storage.name

    @property
    def friendly_name(self) -> str:
        """Return the friendly name of this entry."""
        if self.storage is None:
            return self.name
        return self.storage.friendly_name

    @property
    def comment(self) -> str | None:
        """Return the comment of this entry."""
        if self.storage is None:
            return None
        return self.storage.comment

    @property
    def target_platform(self) -> str | None:
        """Return the target platform of this entry."""
        if self.storage is None:
            return None
        return self.storage.target_platform

    @property
    def update_available(self) -> bool:
        """Return if an update is available for this entry."""
        if self.storage is None:
            return True
        return self.update_old != self.update_new

    @property
    def update_old(self) -> str:
        if self.storage is None:
            return ""
        return self.storage.esphome_version or ""

    @property
    def update_new(self) -> str:
        return const.__version__

    @property
    def loaded_integrations(self) -> set[str]:
        if self.storage is None:
            return []
        return self.storage.loaded_integrations
