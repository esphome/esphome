from __future__ import annotations
import binascii
import codecs
import json
import logging
import os
from datetime import datetime

from esphome import const
from esphome.const import CONF_DISABLED, CONF_MDNS
from esphome.core import CORE
from esphome.helpers import write_file_if_changed
from esphome.types import CoreType

_LOGGER = logging.getLogger(__name__)


def storage_path() -> str:
    return os.path.join(CORE.data_dir, "storage", f"{CORE.config_filename}.json")


def ext_storage_path(config_filename: str) -> str:
    return os.path.join(CORE.data_dir, "storage", f"{config_filename}.json")


def esphome_storage_path() -> str:
    return os.path.join(CORE.data_dir, "esphome.json")


def trash_storage_path() -> str:
    return CORE.relative_config_path("trash")


class StorageJSON:
    def __init__(
        self,
        storage_version: int,
        name: str,
        friendly_name: str,
        comment: str,
        esphome_version: str,
        src_version: int | None,
        address: str,
        web_port: int | None,
        target_platform: str,
        build_path: str,
        firmware_bin_path: str,
        loaded_integrations: set[str],
        no_mdns: bool,
    ) -> None:
        # Version of the storage JSON schema
        assert storage_version is None or isinstance(storage_version, int)
        self.storage_version = storage_version
        # The name of the node
        self.name = name
        # The friendly name of the node
        self.friendly_name = friendly_name
        # The comment of the node
        self.comment = comment
        # The esphome version this was compiled with
        self.esphome_version = esphome_version
        # The version of the file in src/main.cpp - Used to migrate the file
        assert src_version is None or isinstance(src_version, int)
        self.src_version = src_version
        # Address of the ESP, for example livingroom.local or a static IP
        self.address = address
        # Web server port of the ESP, for example 80
        assert web_port is None or isinstance(web_port, int)
        self.web_port = web_port
        # The type of hardware in use, like "ESP32", "ESP32C3", "ESP8266", etc.
        self.target_platform = target_platform
        # The absolute path to the platformio project
        self.build_path = build_path
        # The absolute path to the firmware binary
        self.firmware_bin_path = firmware_bin_path
        # A set of strings of names of loaded integrations
        self.loaded_integrations = loaded_integrations
        # Is mDNS disabled
        self.no_mdns = no_mdns

    def as_dict(self):
        return {
            "storage_version": self.storage_version,
            "name": self.name,
            "friendly_name": self.friendly_name,
            "comment": self.comment,
            "esphome_version": self.esphome_version,
            "src_version": self.src_version,
            "address": self.address,
            "web_port": self.web_port,
            "esp_platform": self.target_platform,
            "build_path": self.build_path,
            "firmware_bin_path": self.firmware_bin_path,
            "loaded_integrations": sorted(self.loaded_integrations),
            "no_mdns": self.no_mdns,
        }

    def to_json(self):
        return f"{json.dumps(self.as_dict(), indent=2)}\n"

    def save(self, path):
        write_file_if_changed(path, self.to_json())

    @staticmethod
    def from_esphome_core(esph: CoreType, old: StorageJSON | None) -> StorageJSON:
        hardware = esph.target_platform.upper()
        if esph.is_esp32:
            from esphome.components import esp32

            hardware = esp32.get_esp32_variant(esph)
        return StorageJSON(
            storage_version=1,
            name=esph.name,
            friendly_name=esph.friendly_name,
            comment=esph.comment,
            esphome_version=const.__version__,
            src_version=1,
            address=esph.address,
            web_port=esph.web_port,
            target_platform=hardware,
            build_path=esph.build_path,
            firmware_bin_path=esph.firmware_bin,
            loaded_integrations=esph.loaded_integrations,
            no_mdns=(
                CONF_MDNS in esph.config
                and CONF_DISABLED in esph.config[CONF_MDNS]
                and esph.config[CONF_MDNS][CONF_DISABLED] is True
            ),
        )

    @staticmethod
    def from_wizard(
        name: str, friendly_name: str, address: str, platform: str
    ) -> StorageJSON:
        return StorageJSON(
            storage_version=1,
            name=name,
            friendly_name=friendly_name,
            comment=None,
            esphome_version=None,
            src_version=1,
            address=address,
            web_port=None,
            target_platform=platform,
            build_path=None,
            firmware_bin_path=None,
            loaded_integrations=set(),
            no_mdns=False,
        )

    @staticmethod
    def _load_impl(path: str) -> StorageJSON | None:
        with codecs.open(path, "r", encoding="utf-8") as f_handle:
            storage = json.load(f_handle)
        storage_version = storage["storage_version"]
        name = storage.get("name")
        friendly_name = storage.get("friendly_name")
        comment = storage.get("comment")
        esphome_version = storage.get(
            "esphome_version", storage.get("esphomeyaml_version")
        )
        src_version = storage.get("src_version")
        address = storage.get("address")
        web_port = storage.get("web_port")
        esp_platform = storage.get("esp_platform")
        build_path = storage.get("build_path")
        firmware_bin_path = storage.get("firmware_bin_path")
        loaded_integrations = set(storage.get("loaded_integrations", []))
        no_mdns = storage.get("no_mdns", False)
        return StorageJSON(
            storage_version,
            name,
            friendly_name,
            comment,
            esphome_version,
            src_version,
            address,
            web_port,
            esp_platform,
            build_path,
            firmware_bin_path,
            loaded_integrations,
            no_mdns,
        )

    @staticmethod
    def load(path: str) -> StorageJSON | None:
        try:
            return StorageJSON._load_impl(path)
        except Exception:  # pylint: disable=broad-except
            return None

    def __eq__(self, o) -> bool:
        return isinstance(o, StorageJSON) and self.as_dict() == o.as_dict()


class EsphomeStorageJSON:
    def __init__(
        self, storage_version, cookie_secret, last_update_check, remote_version
    ):
        # Version of the storage JSON schema
        assert storage_version is None or isinstance(storage_version, int)
        self.storage_version: int = storage_version
        # The cookie secret for the dashboard
        self.cookie_secret: str = cookie_secret
        # The last time ESPHome checked for an update as an isoformat encoded str
        self.last_update_check_str: str = last_update_check
        # Cache of the version gotten in the last version check
        self.remote_version: str | None = remote_version

    def as_dict(self) -> dict:
        return {
            "storage_version": self.storage_version,
            "cookie_secret": self.cookie_secret,
            "last_update_check": self.last_update_check_str,
            "remote_version": self.remote_version,
        }

    @property
    def last_update_check(self) -> datetime | None:
        try:
            return datetime.strptime(self.last_update_check_str, "%Y-%m-%dT%H:%M:%S")
        except Exception:  # pylint: disable=broad-except
            return None

    @last_update_check.setter
    def last_update_check(self, new: datetime) -> None:
        self.last_update_check_str = new.strftime("%Y-%m-%dT%H:%M:%S")

    def to_json(self) -> dict:
        return f"{json.dumps(self.as_dict(), indent=2)}\n"

    def save(self, path: str) -> None:
        write_file_if_changed(path, self.to_json())

    @staticmethod
    def _load_impl(path: str) -> EsphomeStorageJSON | None:
        with codecs.open(path, "r", encoding="utf-8") as f_handle:
            storage = json.load(f_handle)
        storage_version = storage["storage_version"]
        cookie_secret = storage.get("cookie_secret")
        last_update_check = storage.get("last_update_check")
        remote_version = storage.get("remote_version")
        return EsphomeStorageJSON(
            storage_version, cookie_secret, last_update_check, remote_version
        )

    @staticmethod
    def load(path: str) -> EsphomeStorageJSON | None:
        try:
            return EsphomeStorageJSON._load_impl(path)
        except Exception:  # pylint: disable=broad-except
            return None

    @staticmethod
    def get_default() -> EsphomeStorageJSON:
        return EsphomeStorageJSON(
            storage_version=1,
            cookie_secret=binascii.hexlify(os.urandom(64)).decode(),
            last_update_check=None,
            remote_version=None,
        )

    def __eq__(self, o) -> bool:
        return isinstance(o, EsphomeStorageJSON) and self.as_dict() == o.as_dict()
