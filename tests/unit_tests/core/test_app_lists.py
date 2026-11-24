"""Constant App lists and runtime entity reservations (esphome/core/app_lists.py)."""

from pathlib import Path
import re

import pytest

from esphome import codegen as cg
from esphome.core import CORE, ID, EsphomeError, app_lists

ENTITY_TYPES_H = (
    Path(__file__).parents[3] / "esphome" / "core" / "entity_types.h"
).read_text(encoding="utf-8")


def _define_value(name: str) -> str | None:
    for define in CORE.defines:
        if define.name == name:
            return str(define.value)
    return None


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
            "decltype(esphome::APP_SENSOR_LIST) esphome::APP_SENSOR_LIST PROGMEM = {"
            "reinterpret_cast<Sensor *>(esphome__s_b__pstorage), "
            "reinterpret_cast<Sensor *>(esphome__s_a__pstorage)};"
        )
    ]
    assert _define_value("ESPHOME_ENTITY_SENSOR_COUNT") == "2"
    # No reservation: the runtime part is an EmptyList and register_sensor() does not exist
    assert _define_value("ESPHOME_RUNTIME_SENSOR_COUNT") == "0"


def test_reservation_sizes_the_runtime_slots() -> None:
    cg.reserve_runtime_entities("sensor", 3)
    CORE.flush_tasks()
    assert _define_value("ESPHOME_RUNTIME_SENSOR_COUNT") == "3"
    assert _define_value("ESPHOME_ENTITY_SENSOR_COUNT") == "0"
    assert _define_value("USE_SENSOR") is not None
    assert _global_lines() == [
        "decltype(esphome::APP_SENSOR_LIST) esphome::APP_SENSOR_LIST PROGMEM = {nullptr};"
    ]


def test_datetime_reservation_uses_its_own_defines() -> None:
    cg.reserve_runtime_entities("date", 1)
    CORE.flush_tasks()
    assert _define_value("ESPHOME_RUNTIME_DATETIME_DATE_COUNT") == "1"
    assert _define_value("USE_DATETIME_DATE") is not None


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
    with pytest.raises(EsphomeError, match="cg.new_Pvariable"):
        app_lists.add_entity("sensor", runtime)


def test_declared_and_listed_counts_must_match() -> None:
    CORE.platform_counts["sensor"] = 2
    app_lists.add_entity("sensor", _new_sensor("only_one"))
    app_lists.schedule_emit()
    with pytest.raises(EsphomeError, match="2 sensor entities were declared"):
        CORE.flush_tasks()


def test_unit_test_builds_emit_empty_lists() -> None:
    app_lists.testing_setup_not_run()
    CORE.platform_counts["sensor"] = 1
    app_lists.add_entity("sensor", _new_sensor("never_built"))
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert _define_value("ESPHOME_ENTITY_SENSOR_COUNT") == "0"
    assert "never_built};" not in CORE.cpp_global_section
