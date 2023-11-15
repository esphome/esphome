from __future__ import annotations

import hmac
import os
from pathlib import Path

from esphome import util
from esphome.core import CORE
from esphome.helpers import get_bool_env
from esphome.storage_json import ext_storage_path

from .entries import DashboardEntry
from .util import password_hash


def list_dashboard_entries() -> list[DashboardEntry]:
    """List all dashboard entries."""
    return SETTINGS.entries()


class DashboardSettings:
    """Settings for the dashboard."""

    def __init__(self):
        self.config_dir = ""
        self.password_hash = ""
        self.username = ""
        self.using_password = False
        self.on_ha_addon = False
        self.cookie_secret = None
        self.absolute_config_dir = None
        self._entry_cache: dict[
            str, tuple[tuple[int, int, float, int], DashboardEntry]
        ] = {}

    def parse_args(self, args):
        self.on_ha_addon = args.ha_addon
        password = args.password or os.getenv("PASSWORD", "")
        if not self.on_ha_addon:
            self.username = args.username or os.getenv("USERNAME", "")
            self.using_password = bool(password)
        if self.using_password:
            self.password_hash = password_hash(password)
        self.config_dir = args.configuration
        self.absolute_config_dir = Path(self.config_dir).resolve()
        CORE.config_path = os.path.join(self.config_dir, ".")

    @property
    def relative_url(self):
        return os.getenv("ESPHOME_DASHBOARD_RELATIVE_URL", "/")

    @property
    def status_use_ping(self):
        return get_bool_env("ESPHOME_DASHBOARD_USE_PING")

    @property
    def status_use_mqtt(self):
        return get_bool_env("ESPHOME_DASHBOARD_USE_MQTT")

    @property
    def using_ha_addon_auth(self):
        if not self.on_ha_addon:
            return False
        return not get_bool_env("DISABLE_HA_AUTHENTICATION")

    @property
    def using_auth(self):
        return self.using_password or self.using_ha_addon_auth

    @property
    def streamer_mode(self):
        return get_bool_env("ESPHOME_STREAMER_MODE")

    def check_password(self, username, password):
        if not self.using_auth:
            return True
        if username != self.username:
            return False

        # Compare password in constant running time (to prevent timing attacks)
        return hmac.compare_digest(self.password_hash, password_hash(password))

    def rel_path(self, *args):
        joined_path = os.path.join(self.config_dir, *args)
        # Raises ValueError if not relative to ESPHome config folder
        Path(joined_path).resolve().relative_to(self.absolute_config_dir)
        return joined_path

    def list_yaml_files(self) -> list[str]:
        return util.list_yaml_files([self.config_dir])

    def entries(self) -> list[DashboardEntry]:
        """Fetch all dashboard entries, thread-safe."""
        path_to_cache_key: dict[str, tuple[int, int, float, int]] = {}
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
        for file in self.list_yaml_files():
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

        entry_cache = self._entry_cache

        # Remove entries that no longer exist
        removed: list[str] = []
        for file in entry_cache:
            if file not in path_to_cache_key:
                removed.append(file)

        for file in removed:
            entry_cache.pop(file)

        dashboard_entries: list[DashboardEntry] = []
        for file, cache_key in path_to_cache_key.items():
            if cached_entry := entry_cache.get(file):
                entry_key, dashboard_entry = cached_entry
                if entry_key == cache_key:
                    dashboard_entries.append(dashboard_entry)
                    continue

            dashboard_entry = DashboardEntry(file)
            dashboard_entries.append(dashboard_entry)
            entry_cache[file] = (cache_key, dashboard_entry)

        return dashboard_entries


SETTINGS = DashboardSettings()
