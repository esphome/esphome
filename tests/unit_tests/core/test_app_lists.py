"""Constant App lists (esphome/core/app_lists.py)."""

from pathlib import Path
import re

import pytest

from esphome import codegen as cg
from esphome.core import CORE, ID, EsphomeError, app_lists
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
        match=r"cg\.new_Pvariable.*entities cannot be added at run time",
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


def test_directly_defined_use_gets_its_list() -> None:
    """A test or benchmark that defines USE_<X> itself still gets the list and its count."""
    cg.add_define("USE_INFRARED")
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value("ESPHOME_ENTITY_INFRARED_COUNT") == "0"
    assert get_define_value("USE_IR_RF") is not None
    assert (
        "esphome::app_lists::APP_INFRARED_LIST PROGMEM = {nullptr};"
        in CORE.cpp_global_section
    )


@pytest.mark.parametrize("via_build_flag", [False, True])
def test_count_override_is_rejected(via_build_flag: bool) -> None:
    """The esphome-ramses style override would size the walk past the constant list."""
    CORE.platform_counts["sensor"] = 1
    app_lists.add_entity("sensor", _new_sensor("only_one"))
    if via_build_flag:
        CORE.build_flags.add("-DESPHOME_ENTITY_SENSOR_COUNT=32")
    else:
        cg.add_define("ESPHOME_ENTITY_SENSOR_COUNT", 32)
    app_lists.schedule_emit()
    with pytest.raises(EsphomeError, match="entities cannot be added at run time"):
        CORE.flush_tasks()


def test_spaced_build_flag_override_is_rejected() -> None:
    """`-D NAME=value` with a space is the same define PlatformIO passes to the compiler."""
    CORE.platform_counts["sensor"] = 1
    app_lists.add_entity("sensor", _new_sensor("only_one"))
    CORE.build_flags.add("-D ESPHOME_ENTITY_SENSOR_COUNT=32")
    app_lists.schedule_emit()
    with pytest.raises(
        EsphomeError, match="ESPHOME_ENTITY_SENSOR_COUNT is defined as 32"
    ):
        CORE.flush_tasks()
