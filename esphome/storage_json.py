import binascii
import codecs
from datetime import datetime
import json
import logging
import os
from typing import Any, Optional, List

from esphome import const
from esphome.core import CORE
from esphome.helpers import write_file_if_changed

from esphome.types import CoreType

_LOGGER = logging.getLogger(__name__)


def storage_path():  # type: () -> str
    return CORE.relative_internal_path(f"{CORE.config_filename}.json")


def ext_storage_path(base_path, config_filename):  # type: (str, str) -> str
    return os.path.join(base_path, ".esphome", f"{config_filename}.json")


def esphome_storage_path(base_path):  # type: (str) -> str
    return os.path.join(base_path, ".esphome", "esphome.json")


def trash_storage_path(base_path):  # type: (str) -> str
    return os.path.join(base_path, ".esphome", "trash")


# pylint: disable=too-many-instance-attributes
class StorageJSON:
    def __init__(
        self,
        storage_version,
        name,
        comment,
        esphome_version,
        src_version,
        address,
        web_port,
        target_platform,
        build_path,
        firmware_bin_path,
        loaded_integrations,
    ):
        # Version of the storage JSON schema
        assert storage_version is None or isinstance(storage_version, int)
        self.storage_version = storage_version  # type: int
        # The name of the node
        self.name = name  # type: str
        # The comment of the node
        self.comment = comment  # type: str
        # The esphome version this was compiled with
        self.esphome_version = esphome_version  # type: str
        # The version of the file in src/main.cpp - Used to migrate the file
        assert src_version is None or isinstance(src_version, int)
        self.src_version = src_version  # type: int
        # Address of the ESP, for example livingroom.local or a static IP
        self.address = address  # type: str
        # Web server port of the ESP, for example 80
        assert web_port is None or isinstance(web_port, int)
        self.web_port = web_port  # type: int
        # The type of ESP in use, either ESP32 or ESP8266
        self.target_platform = target_platform  # type: str
        # The absolute path to the platformio project
        self.build_path = build_path  # type: str
        # The absolute path to the firmware binary
        self.firmware_bin_path = firmware_bin_path  # type: str
        # A list of strings of names of loaded integrations
        self.loaded_integrations = loaded_integrations  # type: List[str]
        self.loaded_integrations.sort()

    def as_dict(self):
        return {
            "storage_version": self.storage_version,
            "name": self.name,
            "comment": self.comment,
            "esphome_version": self.esphome_version,
            "src_version": self.src_version,
            "address": self.address,
            "web_port": self.web_port,
            "esp_platform": self.target_platform,
            "build_path": self.build_path,
            "firmware_bin_path": self.firmware_bin_path,
            "loaded_integrations": self.loaded_integrations,
        }

    def to_json(self):
        return f"{json.dumps(self.as_dict(), indent=2)}\n"

    def save(self, path):
        write_file_if_changed(path, self.to_json())

    @staticmethod
    def from_esphome_core(
        esph, old
    ):  # type: (CoreType, Optional[StorageJSON]) -> StorageJSON
        return StorageJSON(
            storage_version=1,
            name=esph.name,
            comment=esph.comment,
            esphome_version=const.__version__,
            src_version=1,
            address=esph.address,
            web_port=esph.web_port,
            target_platform=esph.target_platform,
            build_path=esph.build_path,
            firmware_bin_path=esph.firmware_bin,
            loaded_integrations=list(esph.loaded_integrations),
        )

    @staticmethod
    def from_wizard(name, address, esp_platform):
        # type: (str, str, str) -> StorageJSON
        return StorageJSON(
            storage_version=1,
            name=name,
            comment=None,
            esphome_version=const.__version__,
            src_version=1,
            address=address,
            web_port=None,
            target_platform=esp_platform,
            build_path=None,
            firmware_bin_path=None,
            loaded_integrations=[],
        )

    @staticmethod
    def _load_impl(path):  # type: (str) -> Optional[StorageJSON]
        with codecs.open(path, "r", encoding="utf-8") as f_handle:
            storage = json.load(f_handle)
        storage_version = storage["storage_version"]
        name = storage.get("name")
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
        loaded_integrations = storage.get("loaded_integrations", [])
        return StorageJSON(
            storage_version,
            name,
            comment,
            esphome_version,
            src_version,
            address,
            web_port,
            esp_platform,
            build_path,
            firmware_bin_path,
            loaded_integrations,
        )

    @staticmethod
    def load(path):  # type: (str) -> Optional[StorageJSON]
        try:
            return StorageJSON._load_impl(path)
        except Exception:  # pylint: disable=broad-except
            return None

    def __eq__(self, o):  # type: (Any) -> bool
        return isinstance(o, StorageJSON) and self.as_dict() == o.as_dict()


class EsphomeStorageJSON:
    def __init__(
        self, storage_version, cookie_secret, last_update_check, remote_version
    ):
        # Version of the storage JSON schema
        assert storage_version is None or isinstance(storage_version, int)
        self.storage_version = storage_version  # type: int
        # The cookie secret for the dashboard
        self.cookie_secret = cookie_secret  # type: str
        # The last time ESPHome checked for an update as an isoformat encoded str
        self.last_update_check_str = last_update_check  # type: str
        # Cache of the version gotten in the last version check
        self.remote_version = remote_version  # type: Optional[str]

    def as_dict(self):  # type: () -> dict
        return {
            "storage_version": self.storage_version,
            "cookie_secret": self.cookie_secret,
            "last_update_check": self.last_update_check_str,
            "remote_version": self.remote_version,
        }

    @property
    def last_update_check(self):  # type: () -> Optional[datetime]
        try:
            return datetime.strptime(self.last_update_check_str, "%Y-%m-%dT%H:%M:%S")
        except Exception:  # pylint: disable=broad-except
            return None

    @last_update_check.setter
    def last_update_check(self, new):  # type: (datetime) -> None
        self.last_update_check_str = new.strftime("%Y-%m-%dT%H:%M:%S")

    def to_json(self):  # type: () -> dict
        return f"{json.dumps(self.as_dict(), indent=2)}\n"

    def save(self, path):  # type: (str) -> None
        write_file_if_changed(path, self.to_json())

    @staticmethod
    def _load_impl(path):  # type: (str) -> Optional[EsphomeStorageJSON]
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
    def load(path):  # type: (str) -> Optional[EsphomeStorageJSON]
        try:
            return EsphomeStorageJSON._load_impl(path)
        except Exception:  # pylint: disable=broad-except
            return None

    @staticmethod
    def get_default():  # type: () -> EsphomeStorageJSON
        return EsphomeStorageJSON(
            storage_version=1,
            cookie_secret=binascii.hexlify(os.urandom(64)).decode(),
            last_update_check=None,
            remote_version=None,
        )

    def __eq__(self, o):  # type: (Any) -> bool
        return isinstance(o, EsphomeStorageJSON) and self.as_dict() == o.as_dict()
