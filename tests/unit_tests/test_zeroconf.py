"""Unit tests for ``esphome.zeroconf`` device-discovery primitives.

Covers ``DashboardImportDiscovery`` (state transitions for adoption /
import flows) and ``DiscoveredImport`` (TXT-record parse shape). Both
are part of the cross-tool contract between the legacy dashboard and
the new device-builder backend (esphome/device-builder); changes to
the callback signature, the ``import_state`` dict shape, or the
``DiscoveredImport`` field set will break downstream consumers.
"""

from __future__ import annotations

import asyncio
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

import pytest
from zeroconf import ServiceStateChange

from esphome import zeroconf as zc
from esphome.zeroconf import (
    ESPHOME_SERVICE_TYPE,
    AsyncEsphomeZeroconf,
    DashboardImportDiscovery,
    DashboardStatus,
    DiscoveredImport,
    EsphomeZeroconf,
    async_discover_mdns_devices,
    async_resolve_hosts,
    discover_mdns_devices,
)


def _make_service_info(
    package_import_url: str = "github://esphome/example/example.yaml",
    project_name: str = "esphome.example",
    project_version: str = "1.0.0",
    network: str | None = "wifi",
    friendly_name: str | None = "Living Room",
    version: str | None = "2025.1.0",
    ota_signed: str | None = None,
) -> MagicMock:
    """Build a fake ``AsyncServiceInfo`` with the TXT records we care about.

    The real callback path resolves a service via zeroconf and then
    reads ``info.properties`` (a ``dict[bytes, bytes | None]``). Mock
    that shape so we can drive ``_process_service_info`` directly
    without spinning up a real zeroconf instance.
    """
    info = MagicMock()
    properties: dict[bytes, bytes | None] = {
        b"package_import_url": package_import_url.encode(),
        b"project_name": project_name.encode(),
        b"project_version": project_version.encode(),
    }
    if network is not None:
        properties[b"network"] = network.encode()
    if friendly_name is not None:
        properties[b"friendly_name"] = friendly_name.encode()
    if version is not None:
        properties[b"version"] = version.encode()
    if ota_signed is not None:
        properties[b"ota_signed"] = ota_signed.encode()
    info.properties = properties
    info.load_from_cache.return_value = True
    return info


def test_added_service_populates_import_state_and_fires_callback() -> None:
    """An ADD with the required TXT records lands a ``DiscoveredImport`` and notifies.

    Mirrors what both the legacy dashboard and device-builder rely
    on — the callback is the only signal that an importable device
    has appeared on the LAN, and ``import_state`` is the snapshot
    they read on demand.
    """
    on_update = MagicMock()
    discovery = DashboardImportDiscovery(on_update=on_update)

    info = _make_service_info()
    name = f"living-room.{ESPHOME_SERVICE_TYPE}"
    discovery._process_service_info(name, info)

    assert name in discovery.import_state
    entry = discovery.import_state[name]
    assert isinstance(entry, DiscoveredImport)
    assert entry.device_name == "living-room"
    assert entry.package_import_url == "github://esphome/example/example.yaml"
    assert entry.project_name == "esphome.example"
    assert entry.project_version == "1.0.0"
    assert entry.network == "wifi"
    assert entry.friendly_name == "Living Room"
    assert entry.ota_signed is False
    on_update.assert_called_once_with(name, entry)


def test_added_service_without_required_txt_is_ignored() -> None:
    """A device that doesn't carry ``package_import_url`` etc. isn't importable.

    The dashboard browser also fires for plain ``_esphomelib._tcp``
    services that happen to match the type but aren't dashboard
    imports. Those must not land in ``import_state`` or fire the
    update callback — otherwise the dashboard would surface every
    API-enabled device on the LAN as "ready to adopt".
    """
    on_update = MagicMock()
    discovery = DashboardImportDiscovery(on_update=on_update)

    info = MagicMock()
    # Empty TXT records — no import URL, no version. ``version``-only
    # services hit a separate ``update_device_mdns`` path that talks
    # to ``StorageJSON``; that's covered elsewhere.
    info.properties = {}
    info.load_from_cache.return_value = True

    discovery._process_service_info(f"plain.{ESPHOME_SERVICE_TYPE}", info)

    assert discovery.import_state == {}
    on_update.assert_not_called()


def test_repeated_add_does_not_re_fire_callback() -> None:
    """Re-resolving the same service doesn't spam the on_update callback.

    The dashboard re-resolves periodically; without the ``is_new``
    guard, every refresh would fire ``IMPORTABLE_DEVICE_ADDED`` and
    the dashboard's UI would re-render endlessly.
    """
    on_update = MagicMock()
    discovery = DashboardImportDiscovery(on_update=on_update)

    info = _make_service_info()
    name = f"living-room.{ESPHOME_SERVICE_TYPE}"
    discovery._process_service_info(name, info)
    discovery._process_service_info(name, info)

    on_update.assert_called_once()


def test_removed_service_clears_state_and_fires_none_callback() -> None:
    """A ServiceStateChange.Removed pops the entry and notifies with ``None``.

    Both consumers rely on the ``(name, None)`` callback shape to
    distinguish "device gone" from "device updated". Coordinate
    before changing the second-arg semantics.
    """
    on_update = MagicMock()
    discovery = DashboardImportDiscovery(on_update=on_update)

    info = _make_service_info()
    name = f"living-room.{ESPHOME_SERVICE_TYPE}"
    discovery._process_service_info(name, info)
    on_update.reset_mock()

    discovery.browser_callback(
        zeroconf=MagicMock(),
        service_type=ESPHOME_SERVICE_TYPE,
        name=name,
        state_change=ServiceStateChange.Removed,
    )

    assert name not in discovery.import_state
    on_update.assert_called_once_with(name, None)


def test_remove_for_unknown_service_does_not_fire_callback() -> None:
    """A spurious Removed for a service we never tracked is a silent no-op.

    The browser can fire Removed for any matching service type,
    not just the importable ones we're tracking. Don't let those
    confuse the callback consumer.
    """
    on_update = MagicMock()
    discovery = DashboardImportDiscovery(on_update=on_update)

    discovery.browser_callback(
        zeroconf=MagicMock(),
        service_type=ESPHOME_SERVICE_TYPE,
        name=f"never-seen.{ESPHOME_SERVICE_TYPE}",
        state_change=ServiceStateChange.Removed,
    )

    on_update.assert_not_called()


def test_updated_service_for_unknown_name_is_ignored() -> None:
    """Updates without a prior Add don't seed ``import_state``.

    The dashboard counts on Add to introduce the device and Update
    to refresh it. Letting Update silently introduce new state would
    let an unrelated TXT change bypass the Add-time validation.
    """
    on_update = MagicMock()
    discovery = DashboardImportDiscovery(on_update=on_update)

    discovery.browser_callback(
        zeroconf=MagicMock(),
        service_type=ESPHOME_SERVICE_TYPE,
        name=f"living-room.{ESPHOME_SERVICE_TYPE}",
        state_change=ServiceStateChange.Updated,
    )

    assert discovery.import_state == {}
    on_update.assert_not_called()


def test_network_defaults_to_wifi_when_txt_absent() -> None:
    """Older firmware that doesn't broadcast ``network`` defaults to ``wifi``.

    The TXT record was added in a later release; pre-existing
    factory firmwares advertise without it. ``DiscoveredImport``
    has to default cleanly so adoption flows can still produce a
    valid YAML for those devices.
    """
    discovery = DashboardImportDiscovery()
    info = _make_service_info(network=None)
    name = f"older.{ESPHOME_SERVICE_TYPE}"
    discovery._process_service_info(name, info)

    assert discovery.import_state[name].network == "wifi"


def test_ota_signed_txt_is_parsed() -> None:
    """``ota_signed=1`` marks a device that only accepts signed OTA images.

    Lets a consumer check whether it holds a key the device trusts before
    an update is attempted, instead of failing at install time.
    """
    discovery = DashboardImportDiscovery()
    info = _make_service_info(ota_signed="1")
    name = f"signed.{ESPHOME_SERVICE_TYPE}"
    discovery._process_service_info(name, info)

    assert discovery.import_state[name].ota_signed is True


def test_friendly_name_optional() -> None:
    """``friendly_name`` may be ``None`` if the device doesn't broadcast it.

    Both consumers handle the ``None`` case (rendering the device
    name as fallback in the UI). Locking this in keeps the
    optionality explicit so a future refactor doesn't accidentally
    coerce it into an empty string.
    """
    discovery = DashboardImportDiscovery()
    info = _make_service_info(friendly_name=None)
    name = f"no-friendly.{ESPHOME_SERVICE_TYPE}"
    discovery._process_service_info(name, info)

    assert discovery.import_state[name].friendly_name is None


def test_callback_is_optional() -> None:
    """``on_update=None`` lets ``import_state`` track silently.

    Used by callers that read the dict directly rather than
    subscribing to events.
    """
    discovery = DashboardImportDiscovery(on_update=None)
    info = _make_service_info()
    name = f"silent.{ESPHOME_SERVICE_TYPE}"
    discovery._process_service_info(name, info)

    # No callback to assert against; just verify state landed.
    assert name in discovery.import_state


@pytest.mark.parametrize(
    ("state_change", "expected"),
    [
        (ServiceStateChange.Added, {"node": True}),
        (ServiceStateChange.Updated, {"node": True}),
        (ServiceStateChange.Removed, {"node": False}),
    ],
)
def test_dashboard_status_callback(
    state_change: ServiceStateChange, expected: dict[str, bool]
) -> None:
    """Add/Update mark the node online, Remove marks it offline."""
    on_update = MagicMock()
    status = DashboardStatus(on_update)

    status.browser_callback(
        MagicMock(), ESPHOME_SERVICE_TYPE, f"node.{ESPHOME_SERVICE_TYPE}", state_change
    )

    on_update.assert_called_once_with(expected)


def test_browser_callback_uses_cached_info() -> None:
    """A cache hit is processed synchronously with no background task."""
    discovery = DashboardImportDiscovery()
    name = f"cached.{ESPHOME_SERVICE_TYPE}"
    info = _make_service_info()

    with (
        patch("esphome.zeroconf.AsyncServiceInfo", return_value=info),
        patch("esphome.zeroconf.asyncio.create_task") as create_task,
    ):
        discovery.browser_callback(
            MagicMock(), ESPHOME_SERVICE_TYPE, name, ServiceStateChange.Added
        )

    create_task.assert_not_called()
    assert name in discovery.import_state


@pytest.mark.asyncio
async def test_browser_callback_cache_miss_resolves_in_background() -> None:
    """A cache miss schedules an async resolve that lands in ``import_state``."""
    discovery = DashboardImportDiscovery()
    name = f"uncached.{ESPHOME_SERVICE_TYPE}"
    info = _make_service_info()
    info.load_from_cache.return_value = False
    info.async_request = AsyncMock(return_value=True)

    with patch("esphome.zeroconf.AsyncServiceInfo", return_value=info):
        discovery.browser_callback(
            MagicMock(), ESPHOME_SERVICE_TYPE, name, ServiceStateChange.Added
        )
        assert zc._BACKGROUND_TASKS
        await asyncio.gather(*zc._BACKGROUND_TASKS)

    assert name in discovery.import_state
    assert not zc._BACKGROUND_TASKS


@pytest.mark.asyncio
async def test_async_process_service_info_unresolved() -> None:
    """A failed resolve leaves ``import_state`` untouched."""
    discovery = DashboardImportDiscovery()
    info = MagicMock()
    info.async_request = AsyncMock(return_value=False)

    await discovery._async_process_service_info(
        MagicMock(), info, ESPHOME_SERVICE_TYPE, f"x.{ESPHOME_SERVICE_TYPE}"
    )

    assert discovery.import_state == {}


def test_process_service_info_none_is_ignored() -> None:
    """A ``None`` info is dropped without raising."""
    discovery = DashboardImportDiscovery()

    discovery._process_service_info(f"x.{ESPHOME_SERVICE_TYPE}", None)  # type: ignore[arg-type]

    assert discovery.import_state == {}


def test_version_only_service_updates_device_mdns() -> None:
    """A non-import device with a ``version`` TXT record refreshes its storage."""
    discovery = DashboardImportDiscovery()
    info = MagicMock()
    info.properties = {b"version": b"2025.2.0"}

    with patch.object(discovery, "update_device_mdns") as update:
        discovery._process_service_info(f"plain.{ESPHOME_SERVICE_TYPE}", info)

    update.assert_called_once_with("plain", "2025.2.0")
    assert discovery.import_state == {}


def test_update_device_mdns_saves_changed_version(tmp_path: Path) -> None:
    """A differing mDNS version is written back to the stored JSON."""
    storage = MagicMock()
    storage.esphome_version = "2025.1.0"
    path = tmp_path / "node.yaml.json"

    with (
        patch("esphome.zeroconf.ext_storage_path", return_value=path) as ext_path,
        patch("esphome.zeroconf.StorageJSON.load", return_value=storage) as load,
    ):
        DashboardImportDiscovery().update_device_mdns("node", "2025.2.0")

    ext_path.assert_called_once_with("node.yaml")
    load.assert_called_once_with(path)
    assert storage.esphome_version == "2025.2.0"
    storage.save.assert_called_once_with(path)


def test_update_device_mdns_same_version_not_saved() -> None:
    """An unchanged version does not rewrite the storage file."""
    storage = MagicMock()
    storage.esphome_version = "2025.1.0"

    with (
        patch("esphome.zeroconf.ext_storage_path", return_value=Path("x")),
        patch("esphome.zeroconf.StorageJSON.load", return_value=storage),
    ):
        DashboardImportDiscovery().update_device_mdns("node", "2025.1.0")

    storage.save.assert_not_called()


def test_update_device_mdns_missing_storage() -> None:
    """A device without stored JSON is skipped."""
    with (
        patch("esphome.zeroconf.ext_storage_path", return_value=Path("x")),
        patch("esphome.zeroconf.StorageJSON.load", return_value=None),
    ):
        DashboardImportDiscovery().update_device_mdns("node", "2025.1.0")


@pytest.mark.parametrize(
    ("cached", "requested", "timeout", "addresses", "expected"),
    [
        (True, False, 10.0, ["192.168.1.2"], ["192.168.1.2"]),
        (False, True, 10.0, ["192.168.1.2"], ["192.168.1.2"]),
        (False, True, 10.0, [], None),
        (False, False, 0, ["192.168.1.2"], None),
    ],
)
def test_esphome_zeroconf_resolve_host(
    cached: bool,
    requested: bool,
    timeout: float,
    addresses: list[str],
    expected: list[str] | None,
) -> None:
    """``resolve_host`` tries the cache, then a network request if timeout allows."""
    resolver = MagicMock()
    resolver.load_from_cache.return_value = cached
    resolver.request.return_value = requested
    resolver.parsed_scoped_addresses.return_value = addresses

    # Skip Zeroconf.__init__ so no sockets are opened
    with patch("esphome.zeroconf.AddressResolver", return_value=resolver) as cls:
        result = EsphomeZeroconf.__new__(EsphomeZeroconf).resolve_host(
            "node.local", timeout
        )

    cls.assert_called_once_with("node.local.")
    assert result == expected


@pytest.mark.asyncio
async def test_async_resolve_hosts_logs_exceptions() -> None:
    """A resolver that raises is logged and yields an empty address list."""
    good = MagicMock()
    good.load_from_cache.return_value = False
    good.async_request = AsyncMock(return_value=True)
    good.parsed_scoped_addresses.return_value = ["10.0.0.1"]
    bad = MagicMock()
    bad.load_from_cache.return_value = False
    bad.async_request = AsyncMock(side_effect=OSError("boom"))
    bad.parsed_scoped_addresses.return_value = []

    with patch("esphome.zeroconf.AddressResolver", side_effect=[good, bad]):
        result = await async_resolve_hosts(MagicMock(), ["good", "bad"])

    assert result == {"good": ["10.0.0.1"], "bad": []}


@pytest.mark.asyncio
async def test_async_resolve_host_returns_none_when_unresolved() -> None:
    """``async_resolve_host`` maps an empty address list to ``None``."""
    aiozc = AsyncEsphomeZeroconf.__new__(AsyncEsphomeZeroconf)
    aiozc.zeroconf = MagicMock()

    with patch(
        "esphome.zeroconf.async_resolve_hosts",
        new=AsyncMock(return_value={"node": []}),
    ):
        assert await aiozc.async_resolve_host("node") is None

    with patch(
        "esphome.zeroconf.async_resolve_hosts",
        new=AsyncMock(return_value={"node": ["1.2.3.4"]}),
    ):
        assert await aiozc.async_resolve_host("node") == ["1.2.3.4"]


@pytest.mark.asyncio
async def test_async_discover_mdns_devices_init_failure() -> None:
    """A Zeroconf init error is logged and yields an empty result."""
    with patch("esphome.zeroconf.AsyncEsphomeZeroconf", side_effect=OSError("no net")):
        assert await async_discover_mdns_devices("node", timeout=0) == {}


@pytest.mark.asyncio
async def test_async_discover_mdns_devices_end_to_end() -> None:
    """Browse handler filters names, dedupes hosts and results get resolved."""
    aiozc = MagicMock()
    aiozc.async_close = AsyncMock()
    browser = MagicMock()
    browser.async_cancel = AsyncMock()
    handlers: list = []

    def make_browser(_zc: object, _type: str, handlers: list) -> MagicMock:  # noqa: A002
        captured.extend(handlers)
        return browser

    captured: list = handlers
    svc = f".{ESPHOME_SERVICE_TYPE}"

    async def fake_sleep(_timeout: float) -> None:
        handler = captured[0]
        zc_ = MagicMock()
        handler(
            zc_, ESPHOME_SERVICE_TYPE, "node-aabbcc" + svc, ServiceStateChange.Added
        )
        handler(
            zc_, ESPHOME_SERVICE_TYPE, "node-aabbcc" + svc, ServiceStateChange.Updated
        )
        handler(
            zc_, ESPHOME_SERVICE_TYPE, "node-zzzzzz" + svc, ServiceStateChange.Added
        )
        handler(
            zc_, ESPHOME_SERVICE_TYPE, "node-ddeeff" + svc, ServiceStateChange.Removed
        )
        handler(
            zc_, ESPHOME_SERVICE_TYPE, "node-112233" + svc, ServiceStateChange.Added
        )

    with (
        patch("esphome.zeroconf.AsyncEsphomeZeroconf", return_value=aiozc),
        patch("esphome.zeroconf.AsyncServiceBrowser", side_effect=make_browser),
        patch("esphome.zeroconf.asyncio.sleep", new=fake_sleep),
        patch(
            "esphome.zeroconf.async_resolve_hosts",
            new=AsyncMock(
                return_value={
                    "node-aabbcc.local": ["10.0.0.1"],
                    "node-112233.local": [],
                }
            ),
        ) as resolve,
    ):
        result = await async_discover_mdns_devices("node", timeout=1)

    resolve.assert_awaited_once()
    assert result == {"node-112233.local": [], "node-aabbcc.local": ["10.0.0.1"]}
    browser.async_cancel.assert_awaited_once()
    aiozc.async_close.assert_awaited_once()


@pytest.mark.parametrize(
    ("device_name", "expected"),
    [
        ("node-aabbcc", True),
        ("node-aabbc", False),
        ("node-aabbcg", False),
        ("other-aabbcc", False),
    ],
)
def test_is_mac_suffix_match(device_name: str, expected: bool) -> None:
    """Only ``<prefix><6 hex>`` names match."""
    assert zc._is_mac_suffix_match(device_name, "node-") is expected


def test_await_discovery_branches() -> None:
    """Timeout, exception and success results of ``_await_discovery``."""
    runner = MagicMock()
    runner.event.wait.return_value = False
    assert zc._await_discovery(runner, 1.0) == {}

    runner.event.wait.return_value = True
    runner.exception = RuntimeError("bad")
    assert zc._await_discovery(runner, 1.0) == {}

    runner.exception = None
    runner.result = {"a.local": ["1.1.1.1"]}
    assert zc._await_discovery(runner, 1.0) == {"a.local": ["1.1.1.1"]}

    runner.result = None
    assert zc._await_discovery(runner, 1.0) == {}


def test_discover_mdns_devices_runs_runner() -> None:
    """The sync wrapper starts the runner and returns its result."""
    runner = MagicMock()
    with (
        patch("esphome.zeroconf.AsyncThreadRunner", return_value=runner) as cls,
        patch("esphome.zeroconf._await_discovery", return_value={"a.local": []}) as aw,
    ):
        assert discover_mdns_devices("node", timeout=2.0) == {"a.local": []}

    with patch(
        "esphome.zeroconf.async_discover_mdns_devices", new=MagicMock()
    ) as discover:
        cls.call_args.args[0]()
    discover.assert_called_once_with("node", timeout=2.0)
    runner.start.assert_called_once_with()
    aw.assert_called_once_with(runner, 2.0)


def test_dashboard_status_ignores_unknown_state() -> None:
    """A state change that is not Added/Updated/Removed is ignored."""
    on_update = MagicMock()

    DashboardStatus(on_update).browser_callback(
        MagicMock(),
        ESPHOME_SERVICE_TYPE,
        f"node.{ESPHOME_SERVICE_TYPE}",
        None,  # type: ignore[arg-type]
    )

    on_update.assert_not_called()


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("cached", "timeout"),
    [(True, 10.0), (False, 0)],
)
async def test_async_resolve_hosts_skips_request(cached: bool, timeout: float) -> None:
    """Cached hosts, or a zero timeout, never issue a network request."""
    resolver = MagicMock()
    resolver.load_from_cache.return_value = cached
    resolver.async_request = AsyncMock()
    resolver.parsed_scoped_addresses.return_value = ["10.0.0.1"]

    with patch("esphome.zeroconf.AddressResolver", return_value=resolver):
        result = await async_resolve_hosts(MagicMock(), ["node"], timeout)

    resolver.async_request.assert_not_called()
    assert result == {"node": ["10.0.0.1"]}
