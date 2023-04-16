import socket
import threading
import time
from typing import Optional
import logging
from dataclasses import dataclass

from zeroconf import (
    DNSAddress,
    DNSOutgoing,
    DNSRecord,
    DNSQuestion,
    RecordUpdateListener,
    Zeroconf,
    ServiceBrowser,
    ServiceStateChange,
    current_time_millis,
)

_CLASS_IN = 1
_FLAGS_QR_QUERY = 0x0000  # query
_TYPE_A = 1
_LOGGER = logging.getLogger(__name__)


class HostResolver(RecordUpdateListener):
    def __init__(self, name: str):
        self.name = name
        self.address: Optional[bytes] = None

    def update_record(self, zc: Zeroconf, now: float, record: DNSRecord) -> None:
        if record is None:
            return
        if record.type == _TYPE_A:
            assert isinstance(record, DNSAddress)
            if record.name == self.name:
                self.address = record.address

    def request(self, zc: Zeroconf, timeout: float) -> bool:
        now = time.time()
        delay = 0.2
        next_ = now + delay
        last = now + timeout

        try:
            zc.add_listener(self, None)
            while self.address is None:
                if last <= now:
                    # Timeout
                    return False
                if next_ <= now:
                    out = DNSOutgoing(_FLAGS_QR_QUERY)
                    out.add_question(DNSQuestion(self.name, _TYPE_A, _CLASS_IN))
                    zc.send(out)
                    next_ = now + delay
                    delay *= 2

                time.sleep(min(next_, last) - now)
                now = time.time()
        finally:
            zc.remove_listener(self)

        return True


class DashboardStatus(threading.Thread):
    PING_AFTER = 15 * 1000  # Send new mDNS request after 15 seconds
    OFFLINE_AFTER = PING_AFTER * 2  # Offline if no mDNS response after 30 seconds

    def __init__(self, zc: Zeroconf, on_update) -> None:
        threading.Thread.__init__(self)
        self.zc = zc
        self.query_hosts: set[str] = set()
        self.key_to_host: dict[str, str] = {}
        self.stop_event = threading.Event()
        self.query_event = threading.Event()
        self.on_update = on_update

    def request_query(self, hosts: dict[str, str]) -> None:
        self.query_hosts = set(hosts.values())
        self.key_to_host = hosts
        self.query_event.set()

    def stop(self) -> None:
        self.stop_event.set()
        self.query_event.set()

    def host_status(self, key: str) -> bool:
        entries = self.zc.cache.entries_with_name(key)
        if not entries:
            return False
        now = current_time_millis()

        return any(
            (entry.created + DashboardStatus.OFFLINE_AFTER) >= now for entry in entries
        )

    def run(self) -> None:
        while not self.stop_event.is_set():
            self.on_update(
                {key: self.host_status(host) for key, host in self.key_to_host.items()}
            )
            now = current_time_millis()
            for host in self.query_hosts:
                entries = self.zc.cache.entries_with_name(host)
                if not entries or all(
                    (entry.created + DashboardStatus.PING_AFTER) <= now
                    for entry in entries
                ):
                    out = DNSOutgoing(_FLAGS_QR_QUERY)
                    out.add_question(DNSQuestion(host, _TYPE_A, _CLASS_IN))
                    self.zc.send(out)
            self.query_event.wait()
            self.query_event.clear()


ESPHOME_SERVICE_TYPE = "_esphomelib._tcp.local."
TXT_RECORD_PACKAGE_IMPORT_URL = b"package_import_url"
TXT_RECORD_PROJECT_NAME = b"project_name"
TXT_RECORD_PROJECT_VERSION = b"project_version"
TXT_RECORD_NETWORK = b"network"
TXT_RECORD_FRIENDLY_NAME = b"friendly_name"


@dataclass
class DiscoveredImport:
    friendly_name: Optional[str]
    device_name: str
    package_import_url: str
    project_name: str
    project_version: str
    network: str


class DashboardImportDiscovery:
    def __init__(self, zc: Zeroconf) -> None:
        self.zc = zc
        self.service_browser = ServiceBrowser(
            self.zc, ESPHOME_SERVICE_TYPE, [self._on_update]
        )
        self.import_state: dict[str, DiscoveredImport] = {}

    def _on_update(
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
        if service_type != ESPHOME_SERVICE_TYPE:
            return
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

    def cancel(self) -> None:
        self.service_browser.cancel()


class EsphomeZeroconf(Zeroconf):
    def resolve_host(self, host: str, timeout=3.0):
        info = HostResolver(host)
        if info.request(self, timeout):
            return socket.inet_ntoa(info.address)
        return None
