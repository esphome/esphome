import pytest

from esphome import codegen as cg
from esphome.core import CORE, ID, EsphomeError, app_lists, entity_helpers
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


def test_entity_fields_after_the_string_tables_is_rejected() -> None:
    entity_helpers.register_icon("mdi:early")
    CORE.flush_tasks()
    assert cg.entity_fields(icon="mdi:early") != 0
    with pytest.raises(EsphomeError, match="after the string tables were emitted"):
        cg.entity_fields(icon="mdi:late")


def test_entity_fields_validates_the_icon() -> None:
    assert cg.entity_fields(icon="mdi:home") != 0
    with pytest.raises(EsphomeError, match=r"cg\.entity_fields\(icon=\.\.\.\)"):
        cg.entity_fields(icon="not an icon")
