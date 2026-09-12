from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.alpha3 import Alpha3, alpha3_ns
from esphome.components.alpha3.const import CONF_ALPHA3_ID
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


def test_legacy_six_sensor_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("legacy.yaml"))
    for setter in (
        "set_flow_sensor",
        "set_head_sensor",
        "set_speed_sensor",
        "set_power_sensor",
        "set_voltage_sensor",
        "set_current_sensor",
    ):
        assert setter in main_cpp


def test_shared_alpha3_declarations() -> None:
    assert Alpha3 is not None
    assert alpha3_ns is not None
    assert CONF_ALPHA3_ID == "alpha3_id"


def test_all_entities_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("all_entities.yaml"))
    for setter in (
        "set_flow_sensor",
        "set_head_sensor",
        "set_speed_sensor",
        "set_power_sensor",
        "set_voltage_sensor",
        "set_current_sensor",
        "set_operating_hours_sensor",
        "set_energy_sensor",
        "set_starts_sensor",
        "set_alarm_code_sensor",
        "set_warning_code_sensor",
        "set_ready_binary_sensor",
        "set_realized_operation_text_sensor",
        "set_active_control_source_text_sensor",
    ):
        assert f"alpha3_hub->{setter}(" in main_cpp

    for entity, enum_type, enum_value, setter in (
        ("operating_mode", "Select", "OPERATION_MODE", "set_operation_mode_select"),
        ("control_mode", "Select", "CONTROL_MODE", "set_control_mode_select"),
        ("constant_speed", "Number", "CONSTANT_SPEED", "set_setpoint_number"),
        ("constant_pressure", "Number", "CONSTANT_PRESSURE", "set_setpoint_number"),
        (
            "proportional_pressure",
            "Number",
            "PROPORTIONAL_PRESSURE",
            "set_setpoint_number",
        ),
    ):
        type_value = f"alpha3::Alpha3{enum_type}Type::ALPHA3_{enum_type.upper()}_TYPE_{enum_value}"
        assert (
            f"new({entity}) alpha3::Alpha3{enum_type}(alpha3_hub, {type_value})"
            in main_cpp
        )
        arguments = f"{type_value}, {entity}" if enum_type == "Number" else entity
        assert f"alpha3_hub->{setter}({arguments});" in main_cpp

    assert '{"Normal", "Stop", "Min", "Max"}' in main_cpp
    assert (
        '{"Constant pressure", "Proportional pressure", "Constant speed", '
        '"AUTOADAPT radiator", "AUTOADAPT underfloor", '
        '"AUTOADAPT radiator and underfloor"}' in main_cpp
    )
    for entity, maximum, step in (
        ("constant_speed", "10000", "1"),
        ("constant_pressure", "20", "0.01f"),
        ("proportional_pressure", "20", "0.01f"),
    ):
        assert f"{entity}->traits.set_min_value(0);" in main_cpp
        assert f"{entity}->traits.set_max_value({maximum});" in main_cpp
        assert f"{entity}->traits.set_step({step});" in main_cpp


@pytest.mark.parametrize("platform", ["select", "number"])
def test_control_platform_requires_hub(platform: str) -> None:
    from importlib import import_module

    schema = import_module(f"esphome.components.alpha3.{platform}").CONFIG_SCHEMA
    with pytest.raises(cv.Invalid, match="alpha3_id"):
        schema({})


def test_one_number_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("one_number.yaml"))
    assert main_cpp.count("alpha3_hub->set_setpoint_number(") == 1
    assert "ALPHA3_NUMBER_TYPE_CONSTANT_SPEED" in main_cpp
    assert "set_operation_mode_select" not in main_cpp
    assert "set_control_mode_select" not in main_cpp


def test_binary_sensor_requires_hub() -> None:
    from esphome.components.alpha3.binary_sensor import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match="alpha3_id"):
        CONFIG_SCHEMA({})


def test_text_sensor_requires_hub() -> None:
    from esphome.components.alpha3.text_sensor import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match="alpha3_id"):
        CONFIG_SCHEMA({})


def test_wrong_hub_type_is_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("invalid_wrong_hub.yaml")
    result = read_config({})
    assert result is None
    output = capsys.readouterr().out
    assert "not_alpha3" in output
    assert "doesn't inherit from alpha3::Alpha3" in output


def test_daily_entity_metadata(
    component_config_path: Callable[[str], Path],
) -> None:
    CORE.config_path = component_config_path("all_entities.yaml")
    result = read_config({})
    assert not result.errors
    hub = result["sensor"][0]
    for key, unit, decimals, device_class, state_class, category in (
        ("operating_hours", "h", 2, "duration", "total_increasing", ""),
        ("energy", "kWh", 3, "energy", "total_increasing", ""),
        ("starts", "", 0, "", "total_increasing", ""),
        ("alarm_code", "", 0, "", "", "diagnostic"),
        ("warning_code", "", 0, "", "", "diagnostic"),
    ):
        entity = hub[key]
        assert entity.get("unit_of_measurement", "") == unit
        assert entity["accuracy_decimals"] == decimals
        assert entity.get("device_class", "") == device_class
        assert entity.get("state_class", "") == state_class
        assert entity.get("entity_category", "") == category
    ready = result["binary_sensor"][0]["ready"]
    assert ready["device_class"] == "connectivity"
    assert ready["entity_category"] == "diagnostic"
    for key in ("realized_operation", "active_control_source"):
        assert result["text_sensor"][0][key]["entity_category"] == "diagnostic"
    for key in ("operating_mode", "control_mode"):
        assert result["select"][0][key]["entity_category"] == "config"
    for key, unit in (
        ("constant_speed_setpoint", "RPM"),
        ("constant_pressure_setpoint", "m"),
        ("proportional_pressure_setpoint", "m"),
    ):
        entity = result["number"][0][key]
        assert entity["unit_of_measurement"] == unit
        assert entity["entity_category"] == "config"
