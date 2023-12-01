from __future__ import annotations

import hmac
import os
from pathlib import Path
from typing import Any

from esphome.core import CORE
from esphome.helpers import get_bool_env

from .util.password import password_hash


class DashboardSettings:
    """Settings for the dashboard."""

    def __init__(self) -> None:
        self.config_dir: str = ""
        self.password_hash: str = ""
        self.username: str = ""
        self.using_password: bool = False
        self.on_ha_addon: bool = False
        self.cookie_secret: str | None = None
        self.absolute_config_dir: Path | None = None

    def parse_args(self, args: Any) -> None:
        self.on_ha_addon: bool = args.ha_addon
        password = args.password or os.getenv("PASSWORD") or ""
        if not self.on_ha_addon:
            self.username = args.username or os.getenv("USERNAME") or ""
            self.using_password = bool(password)
        if self.using_password:
            self.password_hash = password_hash(password)
        self.config_dir = args.configuration
        self.absolute_config_dir = Path(self.config_dir).resolve()
        CORE.config_path = os.path.join(self.config_dir, ".")

    @property
    def relative_url(self) -> str:
        return os.getenv("ESPHOME_DASHBOARD_RELATIVE_URL") or "/"

    @property
    def status_use_ping(self):
        return get_bool_env("ESPHOME_DASHBOARD_USE_PING")

    @property
    def status_use_mqtt(self) -> bool:
        return get_bool_env("ESPHOME_DASHBOARD_USE_MQTT")

    @property
    def using_ha_addon_auth(self) -> bool:
        if not self.on_ha_addon:
            return False
        return not get_bool_env("DISABLE_HA_AUTHENTICATION")

    @property
    def using_auth(self) -> bool:
        return self.using_password or self.using_ha_addon_auth

    @property
    def streamer_mode(self) -> bool:
        return get_bool_env("ESPHOME_STREAMER_MODE")

    def check_password(self, username: str, password: str) -> bool:
        if not self.using_auth:
            return True
        if username != self.username:
            return False

        # Compare password in constant running time (to prevent timing attacks)
        return hmac.compare_digest(self.password_hash, password_hash(password))

    def rel_path(self, *args: Any) -> str:
        """Return a path relative to the ESPHome config folder."""
        joined_path = os.path.join(self.config_dir, *args)
        # Raises ValueError if not relative to ESPHome config folder
        Path(joined_path).resolve().relative_to(self.absolute_config_dir)
        return joined_path
