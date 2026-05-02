from __future__ import annotations

import asyncio
from collections.abc import Callable
from dataclasses import dataclass
import logging

from zeroconf import (
    AddressResolver,
    IPVersion,
    ServiceInfo,
    ServiceStateChange,
    Zeroconf,
)
from zeroconf.asyncio import AsyncServiceBrowser, AsyncServiceInfo, AsyncZeroconf

from esphome.async_thread import AsyncThreadRunner
from esphome.storage_json import StorageJSON, ext_storage_path

# Length of the MAC suffix appended when name_add_mac_suffix is enabled.
MAC_SUFFIX_LEN = 6
_HEX_CHARS = frozenset("0123456789abcdef")

_LOGGER = logging.getLogger(__name__)

DEFAULT_TIMEOUT = 10.0
DEFAULT_TIMEOUT_MS = DEFAULT_TIMEOUT * 1000

_BACKGROUND_TASKS: set[asyncio.Task] = set()


class DashboardStatus:
    def __init__(self, on_update: Callable[[dict[str, bool | None]], None]) -> None:
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
    """An importable device discovered via mDNS ``_esphomelib._tcp.local.``.

    Used by:
    - esphome.dashboard (legacy dashboard)
    - device-builder (esphome/device-builder) — surfaces these as
      "discovered devices" on the new dashboard's adoption flow.

    Fields are populated from TXT records on the broadcast service
    info (see :class:`DashboardImportDiscovery`). Coordinate before
    adding/removing fields — both consumers persist them.
    """

    friendly_name: str | None
    device_name: str
    package_import_url: str
    project_name: str
    project_version: str
    network: str


class DashboardBrowser(AsyncServiceBrowser):
    """A class to browse for ESPHome nodes."""


class DashboardImportDiscovery:
    """Track importable devices announcing on ``_esphomelib._tcp.local.``.

    Used by:
    - esphome.dashboard (legacy dashboard)
    - device-builder (esphome/device-builder) — wired up alongside
      the dashboard's own ``ServiceBrowser`` to populate the
      "Discovered devices" panel and the adoption flow.

    The class maintains ``import_state: dict[str, DiscoveredImport]``
    keyed by the mDNS service name. ``on_update`` is invoked with
    ``(name, info | None)`` for additions and removals; update events
    refresh ``import_state`` without firing the callback.
    Coordinate before changing the callback signature or the keys
    of ``import_state`` — device-builder reads both directly.
    """

    def __init__(
        self, on_update: Callable[[str, DiscoveredImport | None], None] | None = None
    ) -> None:
        self.import_state: dict[str, DiscoveredImport] = {}
        self.on_update = on_update

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
            removed = self.import_state.pop(name, None)
            if removed and self.on_update:
                self.on_update(name, None)
            return

        if state_change == ServiceStateChange.Updated and name not in self.import_state:
            # Ignore updates for devices that are not in the import state
            return

        info = AsyncServiceInfo(
            service_type,
            name,
        )
        if info.load_from_cache(zeroconf):
            self._process_service_info(name, info)
            return
        task = asyncio.create_task(
            self._async_process_service_info(zeroconf, info, service_type, name)
        )
        _BACKGROUND_TASKS.add(task)
        task.add_done_callback(_BACKGROUND_TASKS.discard)

    async def _async_process_service_info(
        self, zeroconf: Zeroconf, info: AsyncServiceInfo, service_type: str, name: str
    ) -> None:
        """Process a service info."""
        if await info.async_request(zeroconf, timeout=DEFAULT_TIMEOUT_MS):
            self._process_service_info(name, info)

    def _process_service_info(self, name: str, info: ServiceInfo) -> None:
        """Process a service info."""
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

        discovered = DiscoveredImport(
            friendly_name=friendly_name,
            device_name=node_name,
            package_import_url=import_url,
            project_name=project_name,
            project_version=project_version,
            network=network,
        )
        is_new = name not in self.import_state
        self.import_state[name] = discovered
        if is_new and self.on_update:
            self.on_update(name, discovered)

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
    def resolve_host(
        self, host: str, timeout: float = DEFAULT_TIMEOUT
    ) -> list[str] | None:
        """Resolve a host name to an IP address."""
        info = AddressResolver(f"{host.partition('.')[0]}.local.")
        if (
            info.load_from_cache(self)
            or (timeout and info.request(self, timeout * 1000))
        ) and (addresses := info.parsed_scoped_addresses(IPVersion.All)):
            return addresses
        return None


async def async_resolve_hosts(
    zeroconf: Zeroconf, hosts: list[str], timeout: float = DEFAULT_TIMEOUT
) -> dict[str, list[str]]:
    """Resolve ``hosts`` to IPs using a shared ``Zeroconf`` instance.

    Tries the cache synchronously first (so hosts already primed by a recent
    browse return immediately with no network round-trip), then issues
    ``async_request`` for the remaining misses in parallel via
    ``asyncio.gather``. Returns a dict mapping each host to its list of
    addresses (empty list when unresolved). Only ``<short>.local`` form is
    queried, matching the name scheme the resolvers below expect.
    """
    resolvers: dict[str, AddressResolver] = {}
    pending: list[str] = []
    for host in hosts:
        resolver = AddressResolver(f"{host.partition('.')[0]}.local.")
        resolvers[host] = resolver
        if not resolver.load_from_cache(zeroconf):
            pending.append(host)

    if pending and timeout:
        results = await asyncio.gather(
            *(
                resolvers[host].async_request(zeroconf, timeout * 1000)
                for host in pending
            ),
            return_exceptions=True,
        )
        for host, result in zip(pending, results):
            if isinstance(result, BaseException):
                _LOGGER.debug("Failed to resolve %s: %s", host, result)

    return {
        host: resolver.parsed_scoped_addresses(IPVersion.All)
        for host, resolver in resolvers.items()
    }


class AsyncEsphomeZeroconf(AsyncZeroconf):
    """ESPHome-tuned ``AsyncZeroconf`` with a hostname-resolve helper.

    Used by:
    - esphome.dashboard (legacy dashboard)
    - device-builder (esphome/device-builder) — drives both the live
      mDNS browser and the per-sweep ``async_resolve_host`` fallback
      for non-API devices that don't broadcast esphomelib.

    Coordinate before adding required constructor args or changing
    the ``async_resolve_host`` signature — device-builder calls it
    on every ping cycle.
    """

    async def async_resolve_host(
        self, host: str, timeout: float = DEFAULT_TIMEOUT
    ) -> list[str] | None:
        """Resolve a host name to an IP address."""
        addresses = (await async_resolve_hosts(self.zeroconf, [host], timeout))[host]
        return addresses or None


def _is_mac_suffix_match(device_name: str, prefix: str) -> bool:
    """Return True if ``device_name`` is ``prefix`` followed by a 6-char hex MAC."""
    if not device_name.startswith(prefix):
        return False
    suffix = device_name[len(prefix) :]
    return len(suffix) == MAC_SUFFIX_LEN and all(c in _HEX_CHARS for c in suffix)


async def async_discover_mdns_devices(
    base_name: str, timeout: float = 5.0
) -> dict[str, list[str]]:
    """Discover ESPHome devices via mDNS that match the base name + MAC suffix.

    When ``name_add_mac_suffix`` is enabled, devices advertise as
    ``<base_name>-<6-hex-mac>.local``. This function uses a single
    ``AsyncEsphomeZeroconf`` lifecycle to both browse for matching services and
    resolve their IP addresses, so callers get resolved addresses without
    opening a second Zeroconf client.

    Args:
        base_name: The base device name (without MAC suffix).
        timeout: How long to wait for mDNS responses (default 5 seconds).

    Returns:
        Mapping of ``<device>.local`` hostnames to their resolved IP addresses
        (may be empty for a device if resolution failed within the timeout).
    """
    prefix = f"{base_name}-"
    # Preserves insertion order for stable output and deduplicates
    discovered: dict[str, list[str]] = {}

    def on_service_state_change(
        zeroconf: Zeroconf,
        service_type: str,
        name: str,
        state_change: ServiceStateChange,
    ) -> None:
        if state_change not in (ServiceStateChange.Added, ServiceStateChange.Updated):
            return
        device_name = name.partition(".")[0]
        if not _is_mac_suffix_match(device_name, prefix):
            _LOGGER.debug(
                "Ignoring %s (%s): does not match '%s<6-hex>'",
                device_name,
                state_change.name,
                prefix,
            )
            return
        host = f"{device_name}.local"
        if host in discovered:
            return
        discovered[host] = []
        _LOGGER.debug("Discovered %s (%s)", host, state_change.name)

    _LOGGER.debug(
        "Starting mDNS discovery for '%s<mac>.local' (timeout=%.1fs)",
        prefix,
        timeout,
    )
    try:
        aiozc = AsyncEsphomeZeroconf()
    except Exception as err:  # pylint: disable=broad-except
        # Zeroconf init can raise OSError, NonUniqueNameException, etc.
        # Any failure here just means we can't discover — log and move on.
        _LOGGER.warning("mDNS discovery failed to initialize: %s", err)
        return {}

    try:
        browser = AsyncServiceBrowser(
            aiozc.zeroconf,
            ESPHOME_SERVICE_TYPE,
            handlers=[on_service_state_change],
        )
        try:
            await asyncio.sleep(timeout)
        finally:
            await browser.async_cancel()
        _LOGGER.debug(
            "Browse finished: %d device(s) matched '%s<mac>'",
            len(discovered),
            prefix,
        )

        # Resolve each discovered hostname on the SAME Zeroconf instance so
        # we don't spin up a second client. ``async_resolve_hosts`` tries the
        # cache synchronously (the browse usually primes it) before issuing
        # any ``async_request`` in parallel for misses.
        resolved = await async_resolve_hosts(aiozc.zeroconf, list(discovered))
        for host, addresses in resolved.items():
            if addresses:
                discovered[host] = addresses
                _LOGGER.debug("Resolved %s -> %s", host, addresses)
            else:
                _LOGGER.debug("No addresses returned for %s", host)
    finally:
        await aiozc.async_close()

    return dict(sorted(discovered.items()))


def _await_discovery(
    runner: AsyncThreadRunner[dict[str, list[str]]], timeout: float
) -> dict[str, list[str]]:
    """Wait for ``runner`` to finish and return its discovery result.

    Split out of :func:`discover_mdns_devices` so the timeout branch is
    testable without patching ``asyncio`` or ``threading`` internals — a test
    passes a stub whose ``event.wait`` returns ``False``.
    """
    # Give the discovery an extra second over the browse timeout for the
    # resolution + cleanup pass.
    if not runner.event.wait(timeout=timeout + 2.0):
        _LOGGER.warning("mDNS discovery timed out after %.1fs", timeout)
        return {}
    if runner.exception is not None:
        _LOGGER.warning("mDNS discovery failed: %s", runner.exception)
        return {}
    return runner.result or {}


def discover_mdns_devices(base_name: str, timeout: float = 5.0) -> dict[str, list[str]]:
    """Synchronous wrapper around :func:`async_discover_mdns_devices`."""
    runner = AsyncThreadRunner(
        lambda: async_discover_mdns_devices(base_name, timeout=timeout)
    )
    runner.start()
    return _await_discovery(runner, timeout)
