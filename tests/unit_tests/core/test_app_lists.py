"""Constant App lists and runtime entity reservations (esphome/core/app_lists.py)."""

from pathlib import Path
import re

import pytest

from esphome import codegen as cg
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, ID, EsphomeError, app_lists, entity_helpers
from tests.testing_helpers import get_define_value

ENTITY_TYPES_H = (
    Path(__file__).parents[3] / "esphome" / "core" / "entity_types.h"
).read_text(encoding="utf-8")


def _global_lines() -> list[str]:
    return [line for line in CORE.cpp_global_section.splitlines() if "_LIST" in line]


def _new_sensor(name: str) -> cg.MockObj:
    return cg.new_Pvariable(
        ID(name, is_declaration=True, type=cg.esphome_ns.class_("Sensor"))
    )


def test_entity_lists_match_entity_types_h() -> None:
    """Every entity_types.h entry has the same `singular` and `upper` here."""
    entries = re.findall(
        r"ENTITY_(?:CONTROLLER_)?TYPE_\(\s*\w+::\w+,\s*(\w+),\s*\w+,\s*\w+,\s*(\w+)",
        ENTITY_TYPES_H,
    )
    assert dict(entries) == app_lists.ENTITY_LISTS


def test_entities_are_emitted_in_order() -> None:
    CORE.platform_counts["sensor"] = 2
    app_lists.add_entity("sensor", _new_sensor("s_b"))
    app_lists.add_entity("sensor", _new_sensor("s_a"))
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert _global_lines() == [
        (
            "decltype(esphome::app_lists::APP_SENSOR_LIST) esphome::app_lists::APP_SENSOR_LIST PROGMEM = {"
            "reinterpret_cast<Sensor *>(esphome__s_b__pstorage), "
            "reinterpret_cast<Sensor *>(esphome__s_a__pstorage)};"
        )
    ]
    assert get_define_value("ESPHOME_ENTITY_SENSOR_COUNT") == "2"
    # No reservation: the runtime part is an EmptyList and register_sensor() does not exist
    assert get_define_value("ESPHOME_RUNTIME_SENSOR_COUNT") == "0"
    assert get_define_value("USE_RUNTIME_ENTITIES") is None


def test_reservation_sizes_the_runtime_slots() -> None:
    CORE.loaded_integrations.add("sensor")
    cg.reserve_runtime_entities("sensor", 3)
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value("ESPHOME_RUNTIME_SENSOR_COUNT") == "3"
    assert get_define_value("USE_RUNTIME_ENTITIES") is not None
    assert get_define_value("ESPHOME_ENTITY_SENSOR_COUNT") == "0"
    assert get_define_value("USE_SENSOR") is not None
    assert _global_lines() == [
        "decltype(esphome::app_lists::APP_SENSOR_LIST) esphome::app_lists::APP_SENSOR_LIST PROGMEM = {nullptr};"
    ]


def test_datetime_reservation_uses_its_own_defines() -> None:
    CORE.loaded_integrations.add("datetime")
    cg.reserve_runtime_entities("date", 1)
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value("ESPHOME_RUNTIME_DATETIME_DATE_COUNT") == "1"
    assert get_define_value("USE_DATETIME_DATE") is not None


def test_unknown_platform_is_rejected() -> None:
    with pytest.raises(EsphomeError, match="Unknown entity platform"):
        cg.reserve_runtime_entities("not_a_platform", 1)


def test_alias_resolves_to_its_static_object() -> None:
    target = _new_sensor("real_sensor")
    alias = cg.Pvariable(
        ID("alias_sensor", is_declaration=True, type=cg.esphome_ns.class_("Sensor")),
        target,
    )
    CORE.platform_counts["sensor"] = 1
    app_lists.add_entity("sensor", alias)
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert _global_lines()[-1].endswith(
        "PROGMEM = {reinterpret_cast<Sensor *>(esphome__real_sensor__pstorage)};"
    )


def test_runtime_pointer_is_rejected() -> None:
    runtime = cg.Pvariable(
        ID("runtime_sensor", is_declaration=True, type=cg.esphome_ns.class_("Sensor")),
        cg.RawExpression("make_sensor()"),
    )
    with pytest.raises(
        EsphomeError,
        match=r"cg\.new_Pvariable.*App\.register_<platform>\(obj, name\)",
    ):
        app_lists.add_entity("sensor", runtime)


def test_additions_after_emit_are_rejected() -> None:
    app_lists.schedule_emit()
    CORE.flush_tasks()
    with pytest.raises(EsphomeError, match="after the lists were emitted"):
        app_lists.add_entity("sensor", _new_sensor("too_late"))


def test_use_define_adds_the_datetime_prefix() -> None:
    assert app_lists.use_define("sensor") == "USE_SENSOR"
    assert app_lists.use_define("time") == "USE_DATETIME_TIME"
    assert app_lists.use_define("media_source") == "USE_MEDIA_SOURCE"


def test_reservation_needs_the_integration() -> None:
    with pytest.raises(EsphomeError, match=r'AUTO_LOAD = \["datetime"\]'):
        cg.reserve_runtime_entities("time", 1)


def test_reservation_after_emit_is_rejected() -> None:
    CORE.loaded_integrations.add("sensor")
    app_lists.schedule_emit()
    CORE.flush_tasks()
    with pytest.raises(EsphomeError, match="after the lists were emitted"):
        cg.reserve_runtime_entities("sensor", 1)


def test_directly_defined_use_gets_its_list() -> None:
    """A test or benchmark that defines USE_<X> itself still gets the list and both counts."""
    cg.add_define("USE_INFRARED")
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value("ESPHOME_ENTITY_INFRARED_COUNT") == "0"
    assert get_define_value("ESPHOME_RUNTIME_INFRARED_COUNT") == "0"
    assert get_define_value("USE_IR_RF") is not None
    assert (
        "esphome::app_lists::APP_INFRARED_LIST PROGMEM = {nullptr};"
        in CORE.cpp_global_section
    )


def test_runtime_only_radio_frequency_enables_ir_rf() -> None:
    CORE.loaded_integrations.add("radio_frequency")
    cg.reserve_runtime_entities("radio_frequency", 1)
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value("USE_RADIO_FREQUENCY") is not None
    assert get_define_value("USE_IR_RF") is not None


@pytest.mark.parametrize(
    "override",
    [
        "add_define",
        "-DESPHOME_ENTITY_SENSOR_COUNT=32",
        "-D ESPHOME_ENTITY_SENSOR_COUNT=32",
    ],
)
def test_count_override_is_rejected(override: str) -> None:
    """The esphome-ramses style override would size the walk past the constant list;
    `-D NAME=value` with a space is the same define PlatformIO passes to the compiler."""
    CORE.platform_counts["sensor"] = 1
    app_lists.add_entity("sensor", _new_sensor("only_one"))
    if override == "add_define":
        cg.add_define("ESPHOME_ENTITY_SENSOR_COUNT", 32)
    else:
        CORE.build_flags.add(override)
    app_lists.schedule_emit()
    with pytest.raises(
        EsphomeError, match="ESPHOME_ENTITY_SENSOR_COUNT is defined as 32"
    ):
        CORE.flush_tasks()


def test_device_count_override_names_the_yaml_lists() -> None:
    """A device or area count conflict points at the YAML lists, not at entities."""
    app_lists.add_to_list("DEVICE", _new_sensor("dev_one"))
    cg.add_define("ESPHOME_DEVICE_COUNT", 4)
    app_lists.schedule_emit()
    with pytest.raises(EsphomeError, match="devices and areas come from the YAML"):
        CORE.flush_tasks()


@pytest.mark.parametrize("upper", ["DEVICE", "AREA"])
def test_device_and_area_lists_keep_their_order(upper: str) -> None:
    app_lists.add_to_list(upper, _new_sensor("first"))
    app_lists.add_to_list(upper, _new_sensor("second"))
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value(f"ESPHOME_{upper}_COUNT") == "2"
    line = next(line for line in _global_lines() if f"APP_{upper}_LIST PROGMEM" in line)
    assert line.index("__first__") < line.index("__second__")


@pytest.mark.parametrize("upper", ["DEVICE", "AREA"])
def test_device_and_area_lists_are_absent_without_yaml(upper: str) -> None:
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value(f"ESPHOME_{upper}_COUNT") is None
    assert f"APP_{upper}_LIST" not in CORE.cpp_global_section


@pytest.mark.parametrize("upper", ["DEVICE", "AREA"])
def test_device_and_area_count_override_is_rejected(upper: str) -> None:
    app_lists.add_to_list(upper, _new_sensor("only_one"))
    cg.add_define(f"ESPHOME_{upper}_COUNT", 4)
    app_lists.schedule_emit()
    with pytest.raises(
        EsphomeError, match=f"ESPHOME_{upper}_COUNT is defined as 4 elsewhere"
    ):
        CORE.flush_tasks()


@pytest.mark.parametrize(
    ("platform", "count", "helper"),
    [
        (PLATFORM_ESP32, 10, True),
        (PLATFORM_ESP32, 9, False),
        (PLATFORM_ESP8266, 40, False),
    ],
)
def test_configure_helper_only_where_it_saves_flash(
    platform: str, count: int, helper: bool
) -> None:
    """setup() calls Application::configure_<entity>() only for many ESP32 entities of one type."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    CORE.platform_counts["sensor"] = count
    statement = entity_helpers._ConfigureEntityStatement(
        "sensor",
        cg.MockObj("s", "->").configure_entity_("n", 1, 2),
        cg.MockObj("esphome::Application", "::").configure_sensor(
            cg.MockObj("s"), "n", 1, 2
        ),
        "",
    )
    expected = (
        "esphome::Application::configure_sensor(" if helper else "s->configure_entity_("
    )
    assert str(statement).startswith(expected)


@pytest.mark.parametrize("via_build_flag", [False, True])
def test_runtime_count_override_is_rejected_with_a_reservation(
    via_build_flag: bool,
) -> None:
    CORE.loaded_integrations.add("sensor")
    cg.reserve_runtime_entities("sensor", 12)
    if via_build_flag:
        CORE.build_flags.add("-DESPHOME_RUNTIME_SENSOR_COUNT=2")
    else:
        cg.add_define("ESPHOME_RUNTIME_SENSOR_COUNT", 2)
    app_lists.schedule_emit()
    with pytest.raises(
        EsphomeError, match="ESPHOME_RUNTIME_SENSOR_COUNT is defined as 2"
    ):
        CORE.flush_tasks()


@pytest.mark.parametrize("count", [-1, 1.5, "2", True])
def test_reservation_needs_a_non_negative_int(count: object) -> None:
    CORE.loaded_integrations.add("sensor")
    with pytest.raises(EsphomeError, match="needs a non-negative int"):
        cg.reserve_runtime_entities("sensor", count)


def test_reserving_zero_is_a_no_op() -> None:
    CORE.loaded_integrations.add("sensor")
    cg.reserve_runtime_entities("sensor", 0)
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value("ESPHOME_RUNTIME_SENSOR_COUNT") is None


def test_entity_fields_validates_the_entity_category() -> None:
    assert cg.entity_fields(entity_category="DIAGNOSTIC") == cg.entity_fields(
        entity_category="diagnostic"
    )
    with pytest.raises(EsphomeError, match="entity_category"):
        cg.entity_fields(entity_category="not_a_category")
