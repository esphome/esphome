from __future__ import annotations

import asyncio
import logging
import os

from esphome import const, util
from esphome.storage_json import StorageJSON, ext_storage_path

_LOGGER = logging.getLogger(__name__)

DashboardCacheKeyType = tuple[int, int, float, int]


class DashboardEntries:
    """Represents all dashboard entries."""

    __slots__ = ("_loop", "_config_dir", "_entries", "_loaded_entries", "_update_lock")

    def __init__(self, config_dir: str) -> None:
        """Initialize the DashboardEntries."""
        self._loop = asyncio.get_running_loop()
        self._config_dir = config_dir
        # Entries are stored as
        # {
        #   "path/to/file.yaml": DashboardEntry,
        #   ...
        # }
        self._entries: dict[str, DashboardEntry] = {}
        self._loaded_entries = False
        self._update_lock = asyncio.Lock()

    def get(self, path: str) -> DashboardEntry | None:
        """Get an entry by path."""
        return self._entries.get(path)

    async def _async_all(self) -> list[DashboardEntry]:
        """Return all entries."""
        return list(self._entries.values())

    def all(self) -> list[DashboardEntry]:
        """Return all entries."""
        return asyncio.run_coroutine_threadsafe(self._async_all, self._loop).result()

    def async_all(self) -> list[DashboardEntry]:
        """Return all entries."""
        return list(self._entries.values())

    def update_entries(self) -> list[DashboardEntry]:
        """Update the dashboard entries from disk."""
        return asyncio.run_coroutine_threadsafe(
            self.async_update_entries, self._loop
        ).result()

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
        added: dict[DashboardEntry, DashboardCacheKeyType] = {}
        updated: dict[DashboardEntry, DashboardCacheKeyType] = {}
        removed: set[DashboardEntry] = {
            entry
            for filename, entry in self._entries.items()
            if filename not in path_to_cache_key
        }
        entries = self._entries
        for path, cache_key in path_to_cache_key.items():
            if entry := self._entries.get(path):
                if entry.cache_key != cache_key:
                    updated[entry] = cache_key
            else:
                entry = DashboardEntry(path, cache_key)
                added[entry] = cache_key

        if added or updated:
            await self._loop.run_in_executor(
                None, self._load_entries, {**added, **updated}
            )

        for entry in added:
            _LOGGER.debug("Added dashboard entry %s", entry.path)
            entries[entry.path] = entry

        if entry in removed:
            _LOGGER.debug("Removed dashboard entry %s", entry.path)
            entries.pop(entry.path)

        for entry in updated:
            _LOGGER.debug("Updated dashboard entry %s", entry.path)
        # In the future we can fire events when entries are added/removed/updated

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
        # Because there is no lock the cache may
        # get built more than once but that's fine as its still
        # thread-safe and results in orders of magnitude less
        # reads from disk than if we did not cache at all and
        # does not have a lock contention issue.
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


class DashboardEntry:
    """Represents a single dashboard entry.

    This class is thread-safe and read-only.
    """

    __slots__ = ("path", "filename", "_storage_path", "cache_key", "storage")

    def __init__(self, path: str, cache_key: DashboardCacheKeyType) -> None:
        """Initialize the DashboardEntry."""
        self.path = path
        self.filename = os.path.basename(path)
        self._storage_path = ext_storage_path(self.filename)
        self.cache_key = cache_key
        self.storage: StorageJSON | None = None

    def __repr__(self):
        """Return the representation of this entry."""
        return (
            f"DashboardEntry({self.path} "
            f"address={self.address} "
            f"web_port={self.web_port} "
            f"name={self.name} "
            f"no_mdns={self.no_mdns})"
        )

    def load_from_disk(self, cache_key: DashboardCacheKeyType | None = None) -> None:
        """Load this entry from disk."""
        self.storage = StorageJSON.load(self._storage_path)
        if self.cache_key:
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
    def loaded_integrations(self) -> list[str]:
        if self.storage is None:
            return []
        return self.storage.loaded_integrations
