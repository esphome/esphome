"""Tests for the BLE hub provider registry and the missing-hub diagnostics."""

from collections.abc import Callable, Generator
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
def hub_registry() -> Generator[set[str]]:
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


def _set_platform(platform: str | None) -> None:
    core_data = CORE.data.setdefault(KEY_CORE, {})
    if platform is None:
        core_data.pop(KEY_TARGET_PLATFORM, None)
    else:
        core_data[KEY_TARGET_PLATFORM] = platform


# The missing-hub diagnostics: one test per path so a regression in one
# scenario cannot mask the others. The hub binding must fail with a
# tracker-naming message, not use_id's C++-class error, regardless of
# config-step ordering internals.


def test_empty_registry_names_every_in_tree_tracker(hub_registry: set[str]) -> None:
    # The common failure: a fresh CLI process where the tracker was simply
    # forgotten, so no tracker module was ever imported and the registry is
    # empty. The error must still name the in-tree trackers.
    hub_registry.clear()
    CORE.loaded_integrations.clear()
    _set_platform(None)
    with pytest.raises(
        cv.Invalid,
        match="add one of: bk72xx_ble_tracker, esp32_ble_tracker, ln882h_ble_tracker, rp2_ble_tracker",
    ):
        ble_device_base._require_hub(_generated_id())


def test_platform_filters_the_suggested_trackers(hub_registry: set[str]) -> None:
    hub_registry.clear()
    CORE.loaded_integrations.clear()
    _set_platform("esp32")
    with pytest.raises(cv.Invalid, match="add one of: esp32_ble_tracker$"):
        ble_device_base._require_hub(_generated_id())


def test_ble_less_platform_is_not_misdirected(hub_registry: set[str]) -> None:
    # A known platform with no in-tree hub must not be pointed at other
    # platforms' trackers; out-of-tree BLE hubs are not supported.
    hub_registry.clear()
    CORE.loaded_integrations.clear()
    _set_platform("esp8266")
    with pytest.raises(
        cv.Invalid,
        match="No BLE tracker exists for esp8266; BLE components are not supported",
    ):
        ble_device_base._require_hub(_generated_id())


def test_explicit_id_bypasses_the_registry(hub_registry: set[str]) -> None:
    # Explicit ble_hub_id: is the multi-hub disambiguation case; the ID pass
    # owns that diagnosis and its error names the missing id.
    hub_registry.clear()
    CORE.loaded_integrations.clear()
    explicit = ID("my_hub", is_declaration=False, type="ble_device_base::BLEHub")
    assert ble_device_base._require_hub(explicit) is explicit


def test_registered_and_loaded_provider_passes(hub_registry: set[str]) -> None:
    hub_registry.add("esp32_ble_tracker")
    CORE.loaded_integrations.add("esp32_ble_tracker")
    generated = _generated_id()
    assert ble_device_base._require_hub(generated) is generated


def _module_name(path: Path) -> str:
    """Dotted module name for a file under esphome/components."""
    rel = path.relative_to(COMPONENTS_DIR.parent)
    parts = rel.with_suffix("").parts
    if parts[-1] == "__init__":
        parts = parts[:-1]
    return "esphome." + ".".join(parts)


def _hub_component_modules() -> list[str]:
    """Components whose codegen class inherits ble_device_base.BLEHub.

    The source-text pass only selects import candidates (importing all ~900
    component packages is too slow); membership is decided by the class
    hierarchy via MockObjClass.inherits_from on every module whose source
    matched — nested declaring modules included — so a comment mentioning
    BLEHub in a consumer cannot produce a false positive.
    """
    hub_modules = []
    for pkg in sorted(COMPONENTS_DIR.iterdir()):
        if pkg.name == "ble_device_base" or not (pkg / "__init__.py").is_file():
            continue
        matched = [
            path
            for path in pkg.rglob("*.py")
            if "BLEHub" in path.read_text(encoding="utf-8")
        ]
        if not matched:
            continue
        for path in matched:
            mod = import_module(_module_name(path))
            if any(
                isinstance(attr, MockObjClass)
                and attr is not ble_device_base.BLEHub
                and attr.inherits_from(ble_device_base.BLEHub)
                for attr in vars(mod).values()
            ):
                hub_modules.append(pkg.name)
                break
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


def test_add_service_uuid_dispatches_by_width(monkeypatch: pytest.MonkeyPatch) -> None:
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
    # BLE wire order: the 128-bit array must be byte-reversed — as_hex_array
    # in its place would still emit the right setter name and silently never
    # match on-air.
    assert "0x00,0xff,0xee,0xdd" in emitted[2]
    with pytest.raises(ValueError, match="Unsupported UUID format"):
        ble_device_base.add_service_uuid(var, "123")


@pytest.mark.parametrize(
    ("config_name", "define"),
    [
        ("esp32_tracker_only.yaml", "USE_ESP32_BLE_TRACKER"),
        ("rp2_tracker.yaml", "USE_RP2_BLE_TRACKER"),
        ("bk72xx_tracker.yaml", "USE_BK72XX_BLE_TRACKER"),
        ("ln882h_tracker.yaml", "USE_LN882H_BLE_TRACKER"),
    ],
)
def test_every_tracker_emits_its_alias_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_name: str,
    define: str,
) -> None:
    """Each tracker's codegen must emit its USE_*_BLE_TRACKER define - the
    ble_hub_impl.h alias ladder selects on it. Checked through real codegen
    (the other two legs of the invariant, the ladder arm and the defines.h
    mirror, are compile-enforced: a missing arm fails any build containing a
    BLEHub consumer - today bluetooth_proxy, which CI compiles or tidy-parses
    on every tracker platform - and clang-tidy compiles each arm's
    static_assert)."""
    generate_main(component_config_path(config_name))

    assert define in {d.name for d in CORE.defines}, f"{define} not emitted by codegen"
