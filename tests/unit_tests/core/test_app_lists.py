import pytest

from esphome import codegen as cg
from esphome.core import CORE, ID, EsphomeError, app_lists
from tests.testing_helpers import get_define_value


def _global_lines() -> list[str]:
    return [line for line in CORE.cpp_global_section.splitlines() if "_LIST" in line]


def _new_sensor(name: str) -> cg.MockObj:
    return cg.new_Pvariable(
        ID(name, is_declaration=True, type=cg.esphome_ns.class_("Sensor"))
    )


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
    cg.add_define("USE_INFRARED")
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value("ESPHOME_ENTITY_INFRARED_COUNT") == "0"
    assert get_define_value("USE_IR_RF") is not None
    assert (
        "esphome::app_lists::APP_INFRARED_LIST PROGMEM = {nullptr};"
        in CORE.cpp_global_section
    )


@pytest.mark.parametrize(
    ("platform", "count_define", "list_name", "use_defines"),
    [
        ("sensor", "ESPHOME_ENTITY_SENSOR_COUNT", "APP_SENSOR_LIST", ["USE_SENSOR"]),
        (
            "date",
            "ESPHOME_ENTITY_DATE_COUNT",
            "APP_DATETIME_DATE_LIST",
            ["USE_DATETIME_DATE"],
        ),
        (
            "infrared",
            "ESPHOME_ENTITY_INFRARED_COUNT",
            "APP_INFRARED_LIST",
            ["USE_INFRARED", "USE_IR_RF"],
        ),
        (
            "radio_frequency",
            "ESPHOME_ENTITY_RADIO_FREQUENCY_COUNT",
            "APP_RADIO_FREQUENCY_LIST",
            ["USE_RADIO_FREQUENCY", "USE_IR_RF"],
        ),
    ],
)
def test_platform_list_emission(
    platform: str, count_define: str, list_name: str, use_defines: list[str]
) -> None:
    CORE.platform_counts[platform] = 1
    app_lists.add_entity(platform, _new_sensor("one"))
    app_lists.schedule_emit()
    CORE.flush_tasks()
    assert get_define_value(count_define) == "1"
    for define in use_defines:
        assert get_define_value(define) is not None
    assert any(
        f"app_lists::{list_name} PROGMEM = {{" in line for line in _global_lines()
    )


@pytest.mark.parametrize(
    "override",
    [
        "add_define",
        "-DESPHOME_ENTITY_SENSOR_COUNT=32",
        "-D ESPHOME_ENTITY_SENSOR_COUNT=32",
    ],
)
def test_count_override_is_rejected(override: str) -> None:
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
        EsphomeError,
        match=f"ESPHOME_{upper}_COUNT is defined as 4 elsewhere.*devices and areas come from the YAML",
    ):
        CORE.flush_tasks()
