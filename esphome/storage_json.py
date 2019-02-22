import binascii
import codecs
from datetime import datetime, timedelta
import json
import logging
import os
import threading

from esphome import const
from esphome.core import CORE
from esphome.helpers import mkdir_p

# pylint: disable=unused-import, wrong-import-order
from esphome.core import CoreType  # noqa
from typing import Any, Dict, Optional  # noqa


_LOGGER = logging.getLogger(__name__)


def storage_path():  # type: () -> str
    return CORE.relative_path('.esphome', '{}.json'.format(CORE.config_filename))


def ext_storage_path(base_path, config_filename):  # type: (str, str) -> str
    return os.path.join(base_path, '.esphome', '{}.json'.format(config_filename))


def esphome_storage_path(base_path):  # type: (str) -> str
    return os.path.join(base_path, '.esphome', 'esphome.json')


# pylint: disable=too-many-instance-attributes
class StorageJSON(object):
    def __init__(self, storage_version, name, esphome_core_version, esphome_version,
                 src_version, arduino_version, address, esp_platform, board, build_path,
                 firmware_bin_path, use_legacy_ota):
        # Version of the storage JSON schema
        assert storage_version is None or isinstance(storage_version, int)
        self.storage_version = storage_version  # type: int
        # The name of the node
        self.name = name  # type: str
        # The esphome-core version in use
        assert esphome_core_version is None or isinstance(esphome_core_version, dict)
        self.esphome_core_version = esphome_core_version  # type: Dict[str, str]
        # The esphome version this was compiled with
        self.esphome_version = esphome_version  # type: str
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
        # Whether to use legacy OTA, will be off after the first successful flash
        self.use_legacy_ota = use_legacy_ota

    def as_dict(self):
        return {
            'storage_version': self.storage_version,
            'name': self.name,
            'esphome_core_version': self.esphome_core_version,
            'esphome_version': self.esphome_version,
            'src_version': self.src_version,
            'arduino_version': self.arduino_version,
            'address': self.address,
            'esp_platform': self.esp_platform,
            'board': self.board,
            'build_path': self.build_path,
            'firmware_bin_path': self.firmware_bin_path,
            'use_legacy_ota': self.use_legacy_ota,
        }

    def to_json(self):
        return json.dumps(self.as_dict(), indent=2) + u'\n'

    def save(self, path):
        mkdir_p(os.path.dirname(path))
        with codecs.open(path, 'w', encoding='utf-8') as f_handle:
            f_handle.write(self.to_json())

    @staticmethod
    def from_esphome_core(esph, old):  # type: (CoreType, Optional[StorageJSON]) -> StorageJSON
        return StorageJSON(
            storage_version=1,
            name=esph.name,
            esphome_core_version=esph.esphome_core_version,
            esphome_version=const.__version__,
            src_version=1,
            arduino_version=esph.arduino_version,
            address=esph.address,
            esp_platform=esph.esp_platform,
            board=esph.board,
            build_path=esph.build_path,
            firmware_bin_path=esph.firmware_bin,
            use_legacy_ota=True if old is None else old.use_legacy_ota,
        )

    @staticmethod
    def from_wizard(name, address, esp_platform, board):
        # type: (str, str, str, str) -> StorageJSON
        return StorageJSON(
            storage_version=1,
            name=name,
            esphome_core_version=None,
            esphome_version=const.__version__,
            src_version=1,
            arduino_version=None,
            address=address,
            esp_platform=esp_platform,
            board=board,
            build_path=None,
            firmware_bin_path=None,
            use_legacy_ota=False,
        )

    @staticmethod
    def _load_impl(path):  # type: (str) -> Optional[StorageJSON]
        with codecs.open(path, 'r', encoding='utf-8') as f_handle:
            text = f_handle.read()
        storage = json.loads(text, encoding='utf-8')
        storage_version = storage['storage_version']
        name = storage.get('name')
        esphome_core_version = storage.get('esphome_core_version',
                                           storage.get('esphomelib_version'))
        esphome_version = storage.get('esphome_version', storage.get('esphomeyaml_version'))
        src_version = storage.get('src_version')
        arduino_version = storage.get('arduino_version')
        address = storage.get('address')
        esp_platform = storage.get('esp_platform')
        board = storage.get('board')
        build_path = storage.get('build_path')
        firmware_bin_path = storage.get('firmware_bin_path')
        use_legacy_ota = storage.get('use_legacy_ota')
        return StorageJSON(storage_version, name, esphome_core_version, esphome_version,
                           src_version, arduino_version, address, esp_platform, board, build_path,
                           firmware_bin_path, use_legacy_ota)

    @staticmethod
    def load(path):  # type: (str) -> Optional[StorageJSON]
        try:
            return StorageJSON._load_impl(path)
        except Exception:  # pylint: disable=broad-except
            return None

    def __eq__(self, o):  # type: (Any) -> bool
        return isinstance(o, StorageJSON) and self.as_dict() == o.as_dict()


class EsphomeStorageJSON(object):
    def __init__(self, storage_version, cookie_secret, last_update_check,
                 remote_version):
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
            'storage_version': self.storage_version,
            'cookie_secret': self.cookie_secret,
            'last_update_check': self.last_update_check_str,
            'remote_version': self.remote_version,
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
        return json.dumps(self.as_dict(), indent=2) + u'\n'

    def save(self, path):  # type: (str) -> None
        mkdir_p(os.path.dirname(path))
        with codecs.open(path, 'w', encoding='utf-8') as f_handle:
            f_handle.write(self.to_json())

    @staticmethod
    def _load_impl(path):  # type: (str) -> Optional[EsphomeStorageJSON]
        with codecs.open(path, 'r', encoding='utf-8') as f_handle:
            text = f_handle.read()
        storage = json.loads(text, encoding='utf-8')
        storage_version = storage['storage_version']
        cookie_secret = storage.get('cookie_secret')
        last_update_check = storage.get('last_update_check')
        remote_version = storage.get('remote_version')
        return EsphomeStorageJSON(storage_version, cookie_secret, last_update_check,
                                  remote_version)

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
            cookie_secret=binascii.hexlify(os.urandom(64)),
            last_update_check=None,
            remote_version=None,
        )

    def __eq__(self, o):  # type: (Any) -> bool
        return isinstance(o, EsphomeStorageJSON) and self.as_dict() == o.as_dict()

    @property
    def should_do_esphome_update_check(self):  # type: () -> bool
        if self.last_update_check is None:
            return True
        return self.last_update_check + timedelta(days=3) < datetime.utcnow()


class CheckForUpdateThread(threading.Thread):
    def __init__(self, path):
        threading.Thread.__init__(self)
        self._path = path

    @property
    def docs_base(self):
        return 'https://beta.esphome.io' if 'b' in const.__version__ else \
            'https://esphome.io'

    def fetch_remote_version(self):
        import requests

        storage = EsphomeStorageJSON.load(self._path) or \
            EsphomeStorageJSON.get_default()
        if not storage.should_do_esphome_update_check:
            return storage

        req = requests.get('{}/_static/version'.format(self.docs_base))
        req.raise_for_status()
        storage.remote_version = req.text.strip()
        storage.last_update_check = datetime.utcnow()
        storage.save(self._path)
        return storage

    @staticmethod
    def format_version(ver):
        vstr = '.'.join(map(str, ver.version))
        if ver.prerelease:
            vstr += ver.prerelease[0] + str(ver.prerelease[1])
        return vstr

    def cmp_versions(self, storage):
        # pylint: disable=no-name-in-module, import-error
        from distutils.version import StrictVersion

        remote_version = StrictVersion(storage.remote_version)
        self_version = StrictVersion(const.__version__)
        if remote_version > self_version:
            _LOGGER.warning("*" * 80)
            _LOGGER.warning("A new version of ESPHome is available: %s (this is %s)",
                            self.format_version(remote_version), self.format_version(self_version))
            _LOGGER.warning("Changelog: %s/changelog/index.html", self.docs_base)
            _LOGGER.warning("Update Instructions: %s/guides/faq.html"
                            "#how-do-i-update-to-the-latest-version", self.docs_base)
            _LOGGER.warning("*" * 80)

    def run(self):
        try:
            storage = self.fetch_remote_version()
            self.cmp_versions(storage)
        except Exception:  # pylint: disable=broad-except
            pass


def start_update_check_thread(path):
    # dummy call to strptime as python 2.7 has a bug with strptime when importing from threads
    datetime.strptime('20180101', '%Y%m%d')
    thread = CheckForUpdateThread(os.path.abspath(path))
    thread.start()
    return thread
