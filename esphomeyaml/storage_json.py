import codecs
import json
import os

from esphomeyaml.core import CORE
from esphomeyaml.helpers import mkdir_p

# pylint: disable=unused-import, wrong-import-order
from esphomeyaml.core import CoreType  # noqa
from typing import Any, Dict, Optional  # noqa


def storage_path():  # type: () -> str
    return CORE.relative_path('.esphomeyaml', '{}.json'.format(CORE.config_filename))


def ext_storage_path(base_path, config_filename):  # type: (str, str) -> str
    return os.path.join(base_path, '.esphomeyaml', '{}.json'.format(config_filename))


# pylint: disable=too-many-instance-attributes
class StorageJSON(object):
    def __init__(self, storage_version, name, esphomelib_version, src_version,
                 arduino_version, address, esp_platform, board, build_path,
                 firmware_bin_path):
        # Version of the storage JSON schema
        assert storage_version is None or isinstance(storage_version, int)
        self.storage_version = storage_version  # type: int
        # The name of the node
        self.name = name  # type: str
        # The esphomelib version in use
        assert esphomelib_version is None or isinstance(esphomelib_version, dict)
        self.esphomelib_version = esphomelib_version  # type: Dict[str, str]
        # The version of the file in src/main.cpp - Used to migrate the file
        assert src_version is None or isinstance(src_version, int)
        self.src_version = src_version  # type: int
        # The version of the Arduino framework, the build files need to be cleared each time
        # this changes
        self.arduino_version = arduino_version  # type: str
        # Address of the ESP, for example livingroom.local or a static IP
        self.address = address  # type: str
        # The type of ESP in use, either ESP32 or ESP8266
        self.esp_platform = esp_platform  # type: str
        # The ESP board used, for example nodemcuv2
        self.board = board  # type: str
        # The absolute path to the platformio project
        self.build_path = build_path  # type: str
        # The absolute path to the firmware binary
        self.firmware_bin_path = firmware_bin_path  # type: str

    def as_dict(self):
        return {
            'storage_version': self.storage_version,
            'name': self.name,
            'esphomelib_version': self.esphomelib_version,
            'src_version': self.src_version,
            'arduino_version': self.arduino_version,
            'address': self.address,
            'esp_platform': self.esp_platform,
            'board': self.board,
            'build_path': self.build_path,
            'firmware_bin_path': self.firmware_bin_path,
        }

    def to_json(self):
        return json.dumps(self.as_dict(), indent=2) + u'\n'

    def save(self, path):
        mkdir_p(os.path.dirname(path))
        with codecs.open(path, 'w', encoding='utf-8') as f_handle:
            f_handle.write(self.to_json())

    @staticmethod
    def from_esphomeyaml_core(esph):  # type: (CoreType) -> StorageJSON
        return StorageJSON(
            storage_version=1,
            name=esph.name,
            esphomelib_version=esph.esphomelib_version,
            src_version=1,
            arduino_version=esph.arduino_version,
            address=esph.address,
            esp_platform=esph.esp_platform,
            board=esph.board,
            build_path=esph.build_path,
            firmware_bin_path=esph.firmware_bin,
        )

    @staticmethod
    def from_wizard(name, address, esp_platform, board):
        # type: (str, str, str, str) -> StorageJSON
        return StorageJSON(
            storage_version=1,
            name=name,
            esphomelib_version=None,
            src_version=1,
            arduino_version=None,
            address=address,
            esp_platform=esp_platform,
            board=board,
            build_path=None,
            firmware_bin_path=None,
        )

    @staticmethod
    def _load_impl(path):  # type: (str) -> Optional[StorageJSON]
        with codecs.open(path, 'r', encoding='utf-8') as f_handle:
            text = f_handle.read()
        storage = json.loads(text, encoding='utf-8')
        storage_version = storage['storage_version']
        name = storage.get('name')
        esphomelib_version = storage.get('esphomelib_version')
        src_version = storage.get('src_version')
        arduino_version = storage.get('arduino_version')
        address = storage.get('address')
        esp_platform = storage.get('esp_platform')
        board = storage.get('board')
        build_path = storage.get('build_path')
        firmware_bin_path = storage.get('firmware_bin_path')
        return StorageJSON(storage_version, name, esphomelib_version, src_version, arduino_version,
                           address, esp_platform, board, build_path, firmware_bin_path)

    @staticmethod
    def load(path):  # type: (str) -> Optional[StorageJSON]
        try:
            return StorageJSON._load_impl(path)
        except Exception:  # pylint: disable=broad-except
            return None

    def __eq__(self, o):  # type: (Any) -> bool
        return isinstance(o, StorageJSON) and self.as_dict() == o.as_dict()
