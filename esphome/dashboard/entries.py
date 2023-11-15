from __future__ import annotations

import os

from esphome import const
from esphome.storage_json import StorageJSON, ext_storage_path


class DashboardEntry:
    """Represents a single dashboard entry.

    This class is thread-safe and read-only.
    """

    __slots__ = ("path", "_storage", "_loaded_storage")

    def __init__(self, path: str) -> None:
        """Initialize the DashboardEntry."""
        self.path = path
        self._storage = None
        self._loaded_storage = False

    def __repr__(self):
        """Return the representation of this entry."""
        return (
            f"DashboardEntry({self.path} "
            f"address={self.address} "
            f"web_port={self.web_port} "
            f"name={self.name} "
            f"no_mdns={self.no_mdns})"
        )

    @property
    def filename(self):
        """Return the filename of this entry."""
        return os.path.basename(self.path)

    @property
    def storage(self) -> StorageJSON | None:
        """Return the StorageJSON object for this entry."""
        if not self._loaded_storage:
            self._storage = StorageJSON.load(ext_storage_path(self.filename))
            self._loaded_storage = True
        return self._storage

    @property
    def address(self):
        """Return the address of this entry."""
        if self.storage is None:
            return None
        return self.storage.address

    @property
    def no_mdns(self):
        """Return the no_mdns of this entry."""
        if self.storage is None:
            return None
        return self.storage.no_mdns

    @property
    def web_port(self):
        """Return the web port of this entry."""
        if self.storage is None:
            return None
        return self.storage.web_port

    @property
    def name(self):
        """Return the name of this entry."""
        if self.storage is None:
            return self.filename.replace(".yml", "").replace(".yaml", "")
        return self.storage.name

    @property
    def friendly_name(self):
        """Return the friendly name of this entry."""
        if self.storage is None:
            return self.name
        return self.storage.friendly_name

    @property
    def comment(self):
        """Return the comment of this entry."""
        if self.storage is None:
            return None
        return self.storage.comment

    @property
    def target_platform(self):
        """Return the target platform of this entry."""
        if self.storage is None:
            return None
        return self.storage.target_platform

    @property
    def update_available(self):
        """Return if an update is available for this entry."""
        if self.storage is None:
            return True
        return self.update_old != self.update_new

    @property
    def update_old(self):
        if self.storage is None:
            return ""
        return self.storage.esphome_version or ""

    @property
    def update_new(self):
        return const.__version__

    @property
    def loaded_integrations(self):
        if self.storage is None:
            return []
        return self.storage.loaded_integrations
