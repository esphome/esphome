"""Tests for the BLE hub provider registry and the missing-hub diagnostics."""

from importlib import import_module
from pathlib import Path

import pytest

import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import KEY_TARGET_PLATFORM, Platform
from esphome.core import CORE, ID, KEY_CORE
from esphome.cpp_generator import MockObjClass

COMPONENTS_DIR = Path(ble_device_base.__file__).parent.parent


@pytest.fixture
def hub_registry() -> set[str]:
    """Save/restore _HUB_PROVIDERS — a module global with no reset hook.

    CORE state needs no bookkeeping here: conftest's autouse reset_core
    fixture reassigns it after every test.
    """
    saved = set(ble_device_base._HUB_PROVIDERS)
    yield ble_device_base._HUB_PROVIDERS
    ble_device_base._HUB_PROVIDERS.clear()
    ble_device_base._HUB_PROVIDERS.update(saved)


def _generated_id() -> ID:
    """An ID as cv.GenerateID leaves it before the ID-assignment pass."""
    return ID(None, is_declaration=False, type="ble_device_base::BLEHub")


def test_missing_hub_error_is_actionable(hub_registry: set[str]) -> None:
    """The hub binding must fail with a tracker-naming message, not use_id's
    C++-class error, regardless of config-step ordering internals."""
    core_data = CORE.data.setdefault(KEY_CORE, {})
    # The common failure: a fresh CLI process where the tracker was simply
    # forgotten, so no tracker module was ever imported and the registry is
    # empty. The error must still name the in-tree trackers.
    hub_registry.clear()
    CORE.loaded_integrations.clear()
    core_data.pop(KEY_TARGET_PLATFORM, None)
    with pytest.raises(
        cv.Invalid,
        match="add one of: bk72xx_ble_tracker, esp32_ble_tracker, ln882h_ble_tracker, rp2_ble_tracker",
    ):
        ble_device_base._require_hub(_generated_id())

    # With a target platform set, only that platform's tracker is named.
    core_data[KEY_TARGET_PLATFORM] = "esp32"
    with pytest.raises(cv.Invalid, match="add one of: esp32_ble_tracker;"):
        ble_device_base._require_hub(_generated_id())

    # A known platform with no in-tree hub is not misdirected to other
    # platforms' trackers; the external-tracker escape hatch is named.
    core_data[KEY_TARGET_PLATFORM] = "esp8266"
    with pytest.raises(cv.Invalid, match="No in-tree BLE tracker exists for esp8266"):
        ble_device_base._require_hub(_generated_id())
    core_data[KEY_TARGET_PLATFORM] = "esp32"

    # An external tracker registered (module imported) but not configured:
    # named alongside the in-tree ones.
    hub_registry.add("my_external_tracker")
    with pytest.raises(cv.Invalid, match="my_external_tracker"):
        ble_device_base._require_hub(_generated_id())

    # An explicit ble_hub_id: skips the registry entirely — the user may be
    # naming an external tracker it has never heard of; the ID pass owns
    # that diagnosis.
    explicit = ID("my_hub", is_declaration=False, type="ble_device_base::BLEHub")
    assert ble_device_base._require_hub(explicit) is explicit

    # Provider registered AND loaded -> value passes through untouched.
    hub_registry.add("esp32_ble_tracker")
    CORE.loaded_integrations.add("esp32_ble_tracker")
    generated = _generated_id()
    assert ble_device_base._require_hub(generated) is generated


def _hub_component_modules() -> list[str]:
    """Components whose codegen class inherits ble_device_base.BLEHub.

    The source-text pass only selects import candidates (importing all ~900
    component packages is too slow); membership is decided by the class
    hierarchy via MockObjClass.inherits_from, so a comment mentioning BLEHub
    in a consumer cannot produce a false positive, and the declaring module
    inside the package does not matter.
    """
    hub_modules = []
    for pkg in sorted(COMPONENTS_DIR.iterdir()):
        if pkg.name == "ble_device_base" or not (pkg / "__init__.py").is_file():
            continue
        if not any(
            "BLEHub" in path.read_text(encoding="utf-8") for path in pkg.glob("*.py")
        ):
            continue
        mod = import_module(f"esphome.components.{pkg.name}")
        if any(
            isinstance(attr, MockObjClass)
            and attr is not ble_device_base.BLEHub
            and attr.inherits_from(ble_device_base.BLEHub)
            for attr in vars(mod).values()
        ):
            hub_modules.append(pkg.name)
    return hub_modules


def test_every_in_tree_hub_registers_as_provider() -> None:
    """A BLEHub subclass that forgets register_hub_provider() makes _require_hub
    reject valid configs for that platform — fail CI instead of the user."""
    hub_modules = _hub_component_modules()
    assert hub_modules, "hub discovery found no BLEHub subclasses — scan stale?"
    for name in hub_modules:
        assert name in ble_device_base._HUB_PROVIDERS, (
            f"{name} subclasses ble_device_base.BLEHub but never calls "
            "register_hub_provider(); a valid config using it would be rejected"
        )
    # The per-platform error table must know every in-tree hub, keyed by real
    # platform names — a typo'd key would silently route that platform into
    # the no-in-tree-tracker branch.
    assert set(ble_device_base._IN_TREE_HUB_PROVIDERS.values()) == set(hub_modules)
    platforms = {platform.value for platform in Platform}
    assert set(ble_device_base._IN_TREE_HUB_PROVIDERS) <= platforms


def test_ble_device_schema_declares_the_binding_key(hub_registry: set[str]) -> None:
    """Extending BLE_DEVICE_SCHEMA keeps ble_hub_id a declared key on a strict
    schema, for both the generated and the explicit form, and the missing-hub
    rejection surfaces through the schema itself."""
    schema = cv.Schema({}).extend(ble_device_base.BLE_DEVICE_SCHEMA)
    hub_registry.clear()
    CORE.loaded_integrations.discard("esp32_ble_tracker")
    with pytest.raises(cv.Invalid, match="No BLE tracker configured"):
        schema({})
    hub_registry.add("esp32_ble_tracker")
    CORE.loaded_integrations.add("esp32_ble_tracker")
    generated = schema({})[ble_device_base.CONF_BLE_HUB_ID]
    assert isinstance(generated, ID) and generated.id is None
    explicit = schema({"ble_hub_id": "my_hub"})[ble_device_base.CONF_BLE_HUB_ID]
    assert explicit.id == "my_hub"


def test_rename_legacy_hub_id_migrates_the_old_key() -> None:
    validator = ble_device_base.rename_legacy_hub_id("my_sensor")
    migrated = validator({"esp32_ble_id": "tracker1"})
    assert migrated == {ble_device_base.CONF_BLE_HUB_ID: "tracker1"}
    untouched = validator({"name": "x"})
    assert untouched == {"name": "x"}


def test_add_service_uuid_dispatches_by_width(monkeypatch) -> None:
    emitted: list[str] = []
    monkeypatch.setattr(
        "esphome.components.ble_device_base.cg.add", lambda e: emitted.append(str(e))
    )
    var = cg.MockObj("trig")
    ble_device_base.add_service_uuid(var, "11AA")
    ble_device_base.add_service_uuid(var, "11223344")
    ble_device_base.add_service_uuid(var, "11223344-5566-7788-99aa-bbccddeeff00")
    assert "set_service_uuid16" in emitted[0]
    assert "set_service_uuid32" in emitted[1]
    assert "set_service_uuid128" in emitted[2]
    with pytest.raises(cv.Invalid, match="Unsupported UUID format"):
        ble_device_base.add_service_uuid(var, "123")
