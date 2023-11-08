from __future__ import annotations

import logging
from dataclasses import dataclass
from typing import Callable

from zeroconf import (
    IPVersion,
    ServiceBrowser,
    ServiceInfo,
    ServiceStateChange,
    Zeroconf,
)

from esphome.storage_json import StorageJSON, ext_storage_path

_LOGGER = logging.getLogger(__name__)


class HostResolver(ServiceInfo):
    """Resolve a host name to an IP address."""

    @property
    def _is_complete(self) -> bool:
        """The ServiceInfo has all expected properties."""
        return bool(self._ipv4_addresses)


class DashboardStatus:
    def __init__(self, on_update: Callable[[dict[str, bool | None], []]]) -> None:
        """Initialize the dashboard status."""
        self.on_update = on_update

    def browser_callback(
        self,
        zeroconf: Zeroconf,
        service_type: str,
        name: str,
        state_change: ServiceStateChange,
    ) -> None:
        """Handle a service update."""
        short_name = name.partition(".")[0]
        if state_change == ServiceStateChange.Removed:
            self.on_update({short_name: False})
        elif state_change in (ServiceStateChange.Updated, ServiceStateChange.Added):
            self.on_update({short_name: True})


ESPHOME_SERVICE_TYPE = "_esphomelib._tcp.local."
TXT_RECORD_PACKAGE_IMPORT_URL = b"package_import_url"
TXT_RECORD_PROJECT_NAME = b"project_name"
TXT_RECORD_PROJECT_VERSION = b"project_version"
TXT_RECORD_NETWORK = b"network"
TXT_RECORD_FRIENDLY_NAME = b"friendly_name"
TXT_RECORD_VERSION = b"version"


@dataclass
class DiscoveredImport:
    friendly_name: str | None
    device_name: str
    package_import_url: str
    project_name: str
    project_version: str
    network: str


class DashboardBrowser(ServiceBrowser):
    """A class to browse for ESPHome nodes."""


class DashboardImportDiscovery:
    def __init__(self) -> None:
        self.import_state: dict[str, DiscoveredImport] = {}

    def browser_callback(
        self,
        zeroconf: Zeroconf,
        service_type: str,
        name: str,
        state_change: ServiceStateChange,
    ) -> None:
        _LOGGER.debug(
            "service_update: type=%s name=%s state_change=%s",
            service_type,
            name,
            state_change,
        )
        if state_change == ServiceStateChange.Removed:
            self.import_state.pop(name, None)
            return

        if state_change == ServiceStateChange.Updated and name not in self.import_state:
            # Ignore updates for devices that are not in the import state
            return

        info = zeroconf.get_service_info(service_type, name)
        _LOGGER.debug("-> resolved info: %s", info)
        if info is None:
            return
        node_name = name[: -len(ESPHOME_SERVICE_TYPE) - 1]
        required_keys = [
            TXT_RECORD_PACKAGE_IMPORT_URL,
            TXT_RECORD_PROJECT_NAME,
            TXT_RECORD_PROJECT_VERSION,
        ]
        if any(key not in info.properties for key in required_keys):
            # Not a dashboard import device
            version = info.properties.get(TXT_RECORD_VERSION)
            if version is not None:
                version = version.decode()
                self.update_device_mdns(node_name, version)
            return

        import_url = info.properties[TXT_RECORD_PACKAGE_IMPORT_URL].decode()
        project_name = info.properties[TXT_RECORD_PROJECT_NAME].decode()
        project_version = info.properties[TXT_RECORD_PROJECT_VERSION].decode()
        network = info.properties.get(TXT_RECORD_NETWORK, b"wifi").decode()
        friendly_name = info.properties.get(TXT_RECORD_FRIENDLY_NAME)
        if friendly_name is not None:
            friendly_name = friendly_name.decode()

        self.import_state[name] = DiscoveredImport(
            friendly_name=friendly_name,
            device_name=node_name,
            package_import_url=import_url,
            project_name=project_name,
            project_version=project_version,
            network=network,
        )

    def update_device_mdns(self, node_name: str, version: str):
        storage_path = ext_storage_path(node_name + ".yaml")
        storage_json = StorageJSON.load(storage_path)

        if storage_json is not None:
            storage_version = storage_json.esphome_version
            if version != storage_version:
                storage_json.esphome_version = version
                storage_json.save(storage_path)
                _LOGGER.info(
                    "Updated %s with mdns version %s (was %s)",
                    node_name,
                    version,
                    storage_version,
                )


class EsphomeZeroconf(Zeroconf):
    def resolve_host(self, host: str, timeout=3.0):
        """Resolve a host name to an IP address."""
        name = host.partition(".")[0]
        info = HostResolver(f"{name}.{ESPHOME_SERVICE_TYPE}", ESPHOME_SERVICE_TYPE)
        if (info.load_from_cache(self) or info.request(self, timeout * 1000)) and (
            addresses := info.ip_addresses_by_version(IPVersion.V4Only)
        ):
            return str(addresses[0])
        return None
