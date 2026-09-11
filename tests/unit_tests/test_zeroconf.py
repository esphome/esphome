"""Unit tests for ``esphome.zeroconf`` device-discovery primitives.

Covers ``DashboardImportDiscovery`` (state transitions for adoption /
import flows) and ``DiscoveredImport`` (TXT-record parse shape). Both
are part of the cross-tool contract between the legacy dashboard and
the new device-builder backend (esphome/device-builder); changes to
the callback signature, the ``import_state`` dict shape, or the
``DiscoveredImport`` field set will break downstream consumers.
"""

from __future__ import annotations

from unittest.mock import MagicMock

from zeroconf import ServiceStateChange

from esphome.zeroconf import (
    ESPHOME_SERVICE_TYPE,
    DashboardImportDiscovery,
    DiscoveredImport,
)


def _make_service_info(
    package_import_url: str = "github://esphome/example/example.yaml",
    project_name: str = "esphome.example",
    project_version: str = "1.0.0",
    network: str | None = "wifi",
    friendly_name: str | None = "Living Room",
    version: str | None = "2025.1.0",
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
