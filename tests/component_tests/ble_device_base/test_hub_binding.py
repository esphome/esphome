"""Tests for the BLE hub provider registry and the missing-hub diagnostics."""

from importlib import import_module
from pathlib import Path

import pytest

from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import KEY_TARGET_PLATFORM
from esphome.core import CORE, ID, KEY_CORE

COMPONENTS_DIR = Path(ble_device_base.__file__).parent.parent


def _generated_id() -> ID:
    """An ID as cv.GenerateID leaves it before the ID-assignment pass."""
    return ID(None, is_declaration=False, type="ble_device_base::BLEHub")


def test_missing_hub_error_is_actionable() -> None:
    """The hub binding must fail with a tracker-naming message, not use_id's
    C++-class error, regardless of config-step ordering internals."""
    saved_providers = set(ble_device_base._HUB_PROVIDERS)
    saved_loaded = set(CORE.loaded_integrations)
    core_data = CORE.data.setdefault(KEY_CORE, {})
    platform_was_set = KEY_TARGET_PLATFORM in core_data
    saved_platform = core_data.get(KEY_TARGET_PLATFORM)
    try:
        # The common failure: a fresh CLI process where the tracker was simply
        # forgotten, so no tracker module was ever imported and the registry is
        # empty. The error must still name the in-tree trackers.
        ble_device_base._HUB_PROVIDERS.clear()
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
        with pytest.raises(
            cv.Invalid, match="No in-tree BLE tracker exists for esp8266"
        ):
            ble_device_base._require_hub(_generated_id())
        core_data[KEY_TARGET_PLATFORM] = "esp32"

        # An external tracker registered (module imported) but not configured:
        # named alongside the in-tree ones.
        ble_device_base._HUB_PROVIDERS.add("my_external_tracker")
        with pytest.raises(cv.Invalid, match="my_external_tracker"):
            ble_device_base._require_hub(_generated_id())

        # An explicit ble_hub_id: skips the registry entirely — the user may be
        # naming an external tracker it has never heard of; the ID pass owns
        # that diagnosis.
        explicit = ID("my_hub", is_declaration=False, type="ble_device_base::BLEHub")
        assert ble_device_base._require_hub(explicit) is explicit

        # Provider registered AND loaded -> value passes through untouched.
        ble_device_base._HUB_PROVIDERS.add("esp32_ble_tracker")
        CORE.loaded_integrations.add("esp32_ble_tracker")
        generated = _generated_id()
        assert ble_device_base._require_hub(generated) is generated
    finally:
        ble_device_base._HUB_PROVIDERS.clear()
        ble_device_base._HUB_PROVIDERS.update(saved_providers)
        CORE.loaded_integrations.clear()
        CORE.loaded_integrations.update(saved_loaded)
        if platform_was_set:
            core_data[KEY_TARGET_PLATFORM] = saved_platform
        else:
            core_data.pop(KEY_TARGET_PLATFORM, None)


def test_every_in_tree_hub_registers_as_provider() -> None:
    """A BLEHub subclass that forgets register_hub_provider() makes _require_hub
    reject valid configs for that platform — fail CI instead of the user."""
    hub_modules = []
    for init in sorted(COMPONENTS_DIR.glob("*/__init__.py")):
        text = init.read_text(encoding="utf-8")
        # Both spellings: attribute access (ble_device_base.BLEHub) and a
        # direct `from ... import BLEHub` — either declares a hub subclass.
        if (
            "BLEHub" in text
            and "ble_device_base" in text
            and init.parent.name != "ble_device_base"  # defines the class
        ):
            hub_modules.append(init.parent.name)
    assert hub_modules, "marker scan found no BLEHub subclasses — pattern stale?"
    for name in hub_modules:
        import_module(f"esphome.components.{name}")
        assert name in ble_device_base._HUB_PROVIDERS, (
            f"{name} subclasses ble_device_base.BLEHub but never calls "
            "register_hub_provider(); a valid config using it would be rejected"
        )
    # The per-platform error table must know every in-tree hub as well.
    assert set(ble_device_base._IN_TREE_HUB_PROVIDERS.values()) == set(hub_modules)


def test_ble_device_schema_declares_the_binding_key() -> None:
    """Extending BLE_DEVICE_SCHEMA keeps ble_hub_id a declared key on a strict
    schema, for both the generated and the explicit form."""
    saved_providers = set(ble_device_base._HUB_PROVIDERS)
    saved_loaded = set(CORE.loaded_integrations)
    try:
        ble_device_base._HUB_PROVIDERS.add("esp32_ble_tracker")
        CORE.loaded_integrations.add("esp32_ble_tracker")
        schema = cv.Schema({}).extend(ble_device_base.BLE_DEVICE_SCHEMA)
        generated = schema({})[ble_device_base.CONF_BLE_HUB_ID]
        assert isinstance(generated, ID) and generated.id is None
        explicit = schema({"ble_hub_id": "my_hub"})[ble_device_base.CONF_BLE_HUB_ID]
        assert explicit.id == "my_hub"
    finally:
        ble_device_base._HUB_PROVIDERS.clear()
        ble_device_base._HUB_PROVIDERS.update(saved_providers)
        CORE.loaded_integrations.clear()
        CORE.loaded_integrations.update(saved_loaded)


def test_rename_legacy_hub_id_migrates_the_old_key() -> None:
    validator = ble_device_base.rename_legacy_hub_id("my_sensor")
    migrated = validator({"esp32_ble_id": "tracker1"})
    assert migrated == {ble_device_base.CONF_BLE_HUB_ID: "tracker1"}
    untouched = validator({"name": "x"})
    assert untouched == {"name": "x"}


def test_add_service_uuid_dispatches_by_width(monkeypatch) -> None:
    import esphome.codegen as cg

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
