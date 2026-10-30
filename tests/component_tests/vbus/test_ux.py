"""Short controller configurations preserve explicit settings and generic TX."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components import vbus
from esphome.components.vbus.cs_plus_numbers import NUMBER_PARAMETERS
from esphome.components.vbus.cs_plus_selects import SELECT_PARAMETERS
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize("explicit", [False, True])
def test_controller_address_defaults_preserve_explicit_addresses(
    explicit: bool,
) -> None:
    source = {"transactions": {"controller": {"model": "deltasol_cs_plus"}}}
    if explicit:
        source.update(destination=8721, source=32)
    result = vbus.TRANSMIT_SCHEMA(source)
    assert result["destination"] == 8721
    assert result["source"] == 32


def test_generic_tx_still_requires_an_explicit_destination() -> None:
    with pytest.raises(cv.Invalid, match="Set destination for generic TX"):
        vbus.TRANSMIT_SCHEMA({})
    assert vbus.TRANSMIT_SCHEMA({"destination": 16913})["destination"] == 16913


@pytest.mark.parametrize(
    "value,armed",
    [
        ("direct", False),
        ("armed", True),
        ({}, True),
        ({"require_write_arm": False}, False),
    ],
)
def test_write_modes_normalize_to_the_existing_configuration(
    value: object, armed: bool
) -> None:
    result = vbus.CONTROLLER_SCHEMA({"model": "deltasol_cs_plus", "writes": value})
    assert result["writes"]["require_write_arm"] is armed


@pytest.mark.parametrize("value", [True, False, "automatic"])
def test_write_modes_do_not_enable_writes_implicitly(value: object) -> None:
    with pytest.raises(cv.Invalid, match="writes must be"):
        vbus.CONTROLLER_SCHEMA({"model": "deltasol_cs_plus", "writes": value})


def test_short_and_nested_controller_generate_the_same_operations(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
) -> None:
    path = component_config_path("controller.yaml")
    before = generate_main(path)
    source = yaml_util.load_yaml(path)
    hub = source["vbus"]
    hub["controller"] = hub.pop("transmit")["transactions"]["controller"]
    shortened = tmp_path / "short.yaml"
    shortened.write_text(yaml_util.dump(source))
    CORE.reset()
    after = generate_main(shortened)
    for call in [
        "set_destination(8721)",
        "set_source(32)",
        "vbus::ResolDeltaSolCSPlus(controller_bus,",
        "add_on_snapshot_callback(",
        "start_scan()",
        "suspend_polling()",
        "resume_polling()",
    ]:
        assert before.count(call) == after.count(call) > 0


def test_controller_cannot_be_declared_twice() -> None:
    with pytest.raises(cv.Invalid, match="not both"):
        vbus.CONFIG_SCHEMA(
            {
                "controller": {"model": "deltasol_cs_plus"},
                "transmit": {
                    "transactions": {"controller": {"model": "deltasol_cs_plus"}}
                },
            }
        )


@pytest.mark.parametrize(
    "platform,fixture,parameters",
    [
        ("number", "numbers.yaml", NUMBER_PARAMETERS),
        ("select", "selects.yaml", SELECT_PARAMETERS),
    ],
)
def test_grouped_controls_preserve_every_entity_property(
    platform: str,
    fixture: str,
    parameters: dict,
    component_config_path: Callable[[str], Path],
) -> None:
    source = yaml_util.load_yaml(component_config_path(fixture))
    expected = config.validate_config(source, {})
    assert not expected.errors, expected.errors
    group = {"platform": "vbus"}
    for entry in source[platform]:
        group[entry["parameter"]] = {
            k: v
            for k, v in entry.items()
            if k not in ["platform", "controller_id", "parameter"]
        }
    config_path = CORE.config_path
    CORE.reset()
    CORE.config_path = config_path
    source[platform] = [group]
    actual = config.validate_config(source, {})
    assert not actual.errors, actual.errors
    controller = actual[platform][0]["controller_id"]
    expanded = list(vbus.setting_configs(actual[platform][0], parameters))
    assert len(expanded) == len(expected[platform])
    by_parameter = {item["parameter"]: item for item in expected[platform]}
    for entity in expanded:
        before = by_parameter[entity["parameter"]]
        assert str(before["controller_id"]) == str(controller)
        for key, value in entity.items():
            assert str(before[key]) == str(value), (key, before[key], value)


@pytest.mark.parametrize(
    "platform,fixture", [("number", "numbers.yaml"), ("select", "selects.yaml")]
)
def test_grouped_controls_generate_one_listener_per_setting(
    platform: str,
    fixture: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
) -> None:
    source = yaml_util.load_yaml(component_config_path(fixture))
    group = {"platform": "vbus"}
    for entry in source[platform]:
        group[entry["parameter"]] = {
            k: v
            for k, v in entry.items()
            if k not in ["platform", "controller_id", "parameter"]
        }
    count = len(source[platform])
    source[platform] = [group]
    path = tmp_path / "grouped.yaml"
    path.write_text(yaml_util.dump(source))
    code = generate_main(path)
    assert code.count("->register_setting_listener(") == count
    assert code.count(f"vbus::VBusSetting{platform.capitalize()}(") == count


@pytest.mark.parametrize(
    "platform,parameter",
    [("number", "min_pump_speed_1"), ("select", "display_language")],
)
@pytest.mark.parametrize("writes", [True, False])
def test_grouped_controls_infer_defaults_and_require_write_access(
    platform: str,
    parameter: str,
    writes: bool,
    component_config_path: Callable[[str], Path],
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source.pop("number")
    source[platform] = [{"platform": "vbus", parameter: {}}]
    if not writes:
        del source["vbus"]["transmit"]["transactions"]["controller"]["writes"]
    result = config.validate_config(source, {})
    if not writes:
        assert any("controller.writes" in str(e) for e in result.errors)
        return
    assert not result.errors, result.errors
    assert result[platform][0][parameter]["name"]
    if platform == "number":
        assert result[platform][0][parameter]["unit_of_measurement"] == "%"


@pytest.mark.parametrize("platform", ["number", "select"])
@pytest.mark.parametrize(
    "fields",
    [
        {},
        {"unknown_parameter": {}},
        {"parameter": "display_language", "display_language": {}},
    ],
)
def test_empty_unknown_or_mixed_groups_are_rejected(
    platform: str, fields: dict, component_config_path: Callable[[str], Path]
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source.pop("number")
    source[platform] = [{"platform": "vbus", **fields}]
    result = config.validate_config(source, {})
    assert result.errors


@pytest.mark.parametrize(
    "platform,fixture", [("number", "numbers.yaml"), ("select", "selects.yaml")]
)
def test_single_controller_is_inferred_for_all_controls(
    platform: str, fixture: str, component_config_path: Callable[[str], Path]
) -> None:
    source = yaml_util.load_yaml(component_config_path(fixture))
    original = [entry.pop("controller_id") for entry in source[platform]]
    result = config.validate_config(source, {})
    assert not result.errors, result.errors
    assert [str(entry["controller_id"]) for entry in result[platform]] == original


@pytest.mark.parametrize(
    "platform,fixture", [("number", "numbers.yaml"), ("select", "selects.yaml")]
)
def test_inferred_controller_still_requires_explicit_write_access(
    platform: str, fixture: str, component_config_path: Callable[[str], Path]
) -> None:
    source = yaml_util.load_yaml(component_config_path(fixture))
    del source["vbus"]["transmit"]["transactions"]["controller"]["writes"]
    for entry in source[platform]:
        del entry["controller_id"]
    result = config.validate_config(source, {})
    assert any("controller.writes" in str(e) for e in result.errors), result.errors


@pytest.mark.parametrize(
    "platform,fixture", [("number", "numbers.yaml"), ("select", "selects.yaml")]
)
def test_defaults_keep_existing_names_ids_and_custom_metadata(
    platform: str, fixture: str, component_config_path: Callable[[str], Path]
) -> None:
    source = yaml_util.load_yaml(component_config_path(fixture))
    entry = source[platform][0]
    entry["name"] = "My existing control"
    entry["id"] = "existing_control"
    if platform == "number":
        entry["unit_of_measurement"] = ""
    result = config.validate_config(source, {})
    assert not result.errors, result.errors
    actual = result[platform][0]
    assert actual["name"] == "My existing control"
    assert str(actual["id"]) == "existing_control"
    if platform == "number":
        assert actual["unit_of_measurement"] == ""


def test_new_controls_can_use_only_the_parameter_name(
    component_config_path: Callable[[str], Path],
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source["number"] = [{"platform": "vbus", "parameter": "min_pump_speed_1"}]
    source["select"] = [{"platform": "vbus", "parameter": "display_language"}]
    result = config.validate_config(source, {})
    assert not result.errors, result.errors
    number = result["number"][0]
    assert number["name"] == "Minimum pump speed 1"
    assert number["unit_of_measurement"] == "%"
    assert not number.get("internal", False)
    assert result["select"][0]["name"] == "Display language"
    assert result["select"][0]["options"][4] == "Italian"


def test_id_only_controls_remain_internal(
    component_config_path: Callable[[str], Path],
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source["number"] = [
        {"platform": "vbus", "parameter": "system_time", "id": "private_clock"}
    ]
    result = config.validate_config(source, {})
    assert not result.errors, result.errors
    assert result["number"][0]["name"] == "private_clock"
    assert result["number"][0]["internal"]


def test_all_numeric_units_are_defined() -> None:
    assert len(NUMBER_PARAMETERS) == 58
    assert {p.unit for p in NUMBER_PARAMETERS.values()} == {
        "min",
        "s",
        "h",
        "%",
        "K",
        "°C",
        "L/min",
    }


@pytest.mark.parametrize(
    "parameter",
    [
        "system_time",
        "SYSTEM_TIME",
        "thermostat_start_time_1",
        "tube_collector_end_time",
    ],
)
def test_optional_clock_controls_share_the_existing_controller(
    parameter: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source["datetime"] = [{"platform": "vbus", "parameter": parameter}]
    path = tmp_path / "clock.yaml"
    path.write_text(yaml_util.dump(source))
    code = generate_main(path)
    assert "USE_VBUS_SETTING_TIME" in {define.name for define in CORE.defines}
    assert (
        f"vbus::VBusSettingTime(number_controller, {NUMBER_PARAMETERS[parameter.lower()].index})"
        in code
    )
    assert code.count("number_controller->register_setting_listener(") == 59
    assert (
        CORE.config["datetime"][0]["name"]
        == parameter.lower().replace("_", " ").capitalize()
    )


@pytest.mark.parametrize(
    "parameter", ["disinfection_start_time", "loading_break_time", "display_language"]
)
def test_clock_controls_reject_special_values_durations_and_selects(
    parameter: str, component_config_path: Callable[[str], Path]
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source["datetime"] = [{"platform": "vbus", "parameter": parameter, "name": "Clock"}]
    result = config.validate_config(source, {})
    assert result.errors


@pytest.mark.parametrize("explicit", [False, True])
@pytest.mark.parametrize("grouped", [False, True])
def test_multiple_controllers_require_an_explicit_binding(
    explicit: bool, grouped: bool, component_config_path: Callable[[str], Path]
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source["uart"] = [
        {"id": "first_uart", "port": "/dev/null", "baud_rate": 9600},
        {"id": "second_uart", "port": "/dev/null", "baud_rate": 9600},
    ]
    first = source["vbus"]
    first["uart_id"] = "first_uart"
    source["vbus"] = [
        first,
        {
            "id": "second_bus",
            "uart_id": "second_uart",
            "controller": {
                "id": "second_controller",
                "model": "deltasol_cs_plus",
                "writes": {},
            },
        },
    ]
    source["number"] = [
        {"platform": "vbus", "name": "Pump", "parameter": "min_pump_speed_1"}
    ]
    if grouped:
        source["number"] = [{"platform": "vbus", "min_pump_speed_1": {"name": "Pump"}}]
    if explicit:
        source["number"][0]["controller_id"] = "second_controller"
    result = config.validate_config(source, {})
    if explicit:
        assert not result.errors, result.errors
        assert str(result["number"][0]["controller_id"]) == "second_controller"
    else:
        assert result.errors


@pytest.mark.parametrize("filtered", [False, True])
def test_write_result_sensor_is_optional_and_uses_the_inferred_controller(
    filtered: bool,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source["text_sensor"] = [
        {"platform": "vbus", "last_write_result": {"name": "Last setting write"}}
    ]
    if filtered:
        source["text_sensor"][0]["last_write_result"]["filters"] = [{"to_upper": {}}]
    path = tmp_path / "write-result.yaml"
    path.write_text(yaml_util.dump(source))
    code = generate_main(path)
    assert "USE_VBUS_WRITE_RESULT" in {define.name for define in CORE.defines}
    assert "number_controller->set_last_write_result_text_sensor(" in code
    assert f"vbus::VBusWriteResultTextSensor({'true' if filtered else 'false'})" in code


def test_duplicate_write_result_sensors_are_rejected(
    component_config_path: Callable[[str], Path],
) -> None:
    source = yaml_util.load_yaml(component_config_path("numbers.yaml"))
    source["text_sensor"] = [
        {"platform": "vbus", "last_write_result": {"name": name}}
        for name in ["First result", "Second result"]
    ]
    result = config.validate_config(source, {})
    assert any("only once per controller" in str(e) for e in result.errors), (
        result.errors
    )
