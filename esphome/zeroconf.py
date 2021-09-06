import socket
import threading
import time
from typing import Dict, Optional

from zeroconf import (
    _CLASS_IN,
    _FLAGS_QR_QUERY,
    _TYPE_A,
    DNSAddress,
    DNSOutgoing,
    DNSRecord,
    DNSQuestion,
    RecordUpdateListener,
    Zeroconf,
)


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

                zc.wait(min(next_, last) - now)
                now = time.time()
        finally:
            zc.remove_listener(self)

        return True


class DashboardStatus(RecordUpdateListener, threading.Thread):
    PING_AFTER = 15 * 1000  # Send new mDNS request after 15 seconds
    OFFLINE_AFTER = PING_AFTER * 2  # Offline if no mDNS response after 30 seconds

    def __init__(self, zc: Zeroconf, on_update) -> None:
        threading.Thread.__init__(self)
        self.zc = zc
        self.query_hosts: set[str] = set()
        self.key_to_host: Dict[str, str] = {}
        self.stop_event = threading.Event()
        self.query_event = threading.Event()
        self.on_update = on_update

    def update_record(self, zc: Zeroconf, now: float, record: DNSRecord) -> None:
        pass

    def request_query(self, hosts: Dict[str, str]) -> None:
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
        now = time.time() * 1000

        return any(
            (entry.created + DashboardStatus.OFFLINE_AFTER) >= now for entry in entries
        )

    def run(self) -> None:
        self.zc.add_listener(self, None)
        while not self.stop_event.is_set():
            self.on_update(
                {key: self.host_status(host) for key, host in self.key_to_host.items()}
            )
            now = time.time() * 1000
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
        self.zc.remove_listener(self)


class EsphomeZeroconf(Zeroconf):
    def resolve_host(self, host: str, timeout=3.0):
        info = HostResolver(host)
        if info.request(self, timeout):
            return socket.inet_ntoa(info.address)
        return None
