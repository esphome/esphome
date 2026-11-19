"""Tests for the climate visual temperature conversion."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.climate import (
    Climate,
    _final_validate,
    _visual_temperatures_in_entity_unit,
    climate_ns,
    climate_schema,
    visual_temperature_step,
)
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT_TEMPERATURE,
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_NAME,
    CONF_TARGET_TEMPERATURE,
    CONF_TEMPERATURE_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VISUAL,
    UNIT_CELSIUS,
    UNIT_FAHRENHEIT,
    UNIT_KELVIN,
)
from esphome.core import CORE, ID, entity_helpers
from esphome.core.entity_helpers import (
    entity_temperature_unit,
    entity_temperature_unit_of_id,
    setup_entity_temperature_unit,
    temperature_in_entity_unit,
)
from esphome.cpp_generator import MockObj


def test_visual_temperature_step_scalar_applies_to_both() -> None:
    step = visual_temperature_step("1°F")
    assert step == {
        CONF_TARGET_TEMPERATURE: (1.0, UNIT_FAHRENHEIT),
        CONF_CURRENT_TEMPERATURE: (1.0, UNIT_FAHRENHEIT),
    }


def test_visual_temperatures_default_to_celsius() -> None:
    config = _visual_temperatures_in_entity_unit(
        {
            CONF_VISUAL: {
                CONF_MIN_TEMPERATURE: (50.0, UNIT_FAHRENHEIT),
                CONF_MAX_TEMPERATURE: (30.0, None),
                CONF_TEMPERATURE_STEP: visual_temperature_step("1.8°F"),
            }
        }
    )
    visual = config[CONF_VISUAL]
    assert visual[CONF_MIN_TEMPERATURE] == pytest.approx(10.0)
    assert visual[CONF_MAX_TEMPERATURE] == 30.0
    assert visual[CONF_TEMPERATURE_STEP][CONF_TARGET_TEMPERATURE] == pytest.approx(1.0)


def test_visual_temperatures_follow_unit_of_measurement() -> None:
    config = _visual_temperatures_in_entity_unit(
        {
            CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT),
            CONF_VISUAL: {
                CONF_MIN_TEMPERATURE: (60.0, None),
                CONF_MAX_TEMPERATURE: (30.0, UNIT_CELSIUS),
                CONF_TEMPERATURE_STEP: {
                    CONF_TARGET_TEMPERATURE: (1.0, UNIT_FAHRENHEIT),
                    CONF_CURRENT_TEMPERATURE: (0.5, UNIT_CELSIUS),
                },
            },
        }
    )
    visual = config[CONF_VISUAL]
    assert visual[CONF_MIN_TEMPERATURE] == 60.0
    assert visual[CONF_MAX_TEMPERATURE] == pytest.approx(86.0)
    assert visual[CONF_TEMPERATURE_STEP][CONF_TARGET_TEMPERATURE] == 1.0
    assert visual[CONF_TEMPERATURE_STEP][CONF_CURRENT_TEMPERATURE] == pytest.approx(0.9)


def test_only_a_validated_unit_is_the_entity_unit() -> None:
    # anova validates its own unit_of_measurement for the device; it is not the entity unit
    assert entity_temperature_unit({CONF_UNIT_OF_MEASUREMENT: "f"}) == UNIT_CELSIUS
    assert (
        entity_temperature_unit({CONF_UNIT_OF_MEASUREMENT: UNIT_FAHRENHEIT})
        == UNIT_CELSIUS
    )
    assert (
        entity_temperature_unit(
            {CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT)}
        )
        == UNIT_FAHRENHEIT
    )


def test_visual_temperatures_converted_once() -> None:
    config = {CONF_VISUAL: {CONF_MIN_TEMPERATURE: (50.0, UNIT_FAHRENHEIT)}}
    _visual_temperatures_in_entity_unit(config)
    _visual_temperatures_in_entity_unit(config)
    assert config[CONF_VISUAL][CONF_MIN_TEMPERATURE] == pytest.approx(10.0)


def test_a_fixed_unit_keeps_visual_temperatures_in_it() -> None:
    # friedrich reports in °F through its traits; the unit is declared, not a user option
    fixed = climate_ns.class_("FixedUnitClimate", Climate)
    schema = climate_schema(fixed, fixed_temperature_unit=UNIT_FAHRENHEIT)
    config = schema({CONF_NAME: "AC", CONF_VISUAL: {CONF_TEMPERATURE_STEP: "1°F"}})
    assert entity_temperature_unit(config) == UNIT_FAHRENHEIT
    assert config[CONF_VISUAL][CONF_TEMPERATURE_STEP][CONF_TARGET_TEMPERATURE] == 1.0
    # Nothing is stored in the config, so a dumped config (without its generated id) validates again
    assert CONF_UNIT_OF_MEASUREMENT not in config
    again = schema(
        {key: value for key, value in config.items() if key != CONF_ID}
        | {CONF_NAME: "AC again"}
    )
    assert again[CONF_VISUAL] == config[CONF_VISUAL]
    with pytest.raises(cv.Invalid, match="unit_of_measurement"):
        schema({CONF_NAME: "AC", CONF_UNIT_OF_MEASUREMENT: UNIT_FAHRENHEIT})


@pytest.mark.parametrize(
    "declared",
    [UNIT_FAHRENHEIT, lambda config, full: UNIT_FAHRENHEIT],
    ids=["fixed", "resolved"],
)
def test_a_subclass_reports_in_its_parents_declared_unit(
    declared: str | Callable[[dict, Config], str], monkeypatch: pytest.MonkeyPatch
) -> None:
    """An external platform built on a declared class (friedrich) keeps the parent's unit."""
    monkeypatch.setattr(entity_helpers, "_FIXED_TEMPERATURE_UNITS", {})
    parent = climate_ns.class_("DeclaredUnitClimate", Climate)
    climate_schema(parent, fixed_temperature_unit=declared)
    child = climate_ns.class_("ExternalClimate", parent)
    grandchild = climate_ns.class_("VendoredClimate", child)
    for class_ in (child, grandchild):
        config = {CONF_ID: ID("ac", is_declaration=True, type=class_)}
        assert entity_temperature_unit(config, Config()) == UNIT_FAHRENHEIT
    # The closest declaration wins
    climate_schema(child, fixed_temperature_unit=UNIT_KELVIN)
    config = {CONF_ID: ID("ac", is_declaration=True, type=grandchild)}
    assert entity_temperature_unit(config, Config()) == UNIT_KELVIN
    # An undeclared class still reports in Celsius
    unrelated = climate_ns.class_("UnrelatedClimate", Climate)
    config = {CONF_ID: ID("ac", is_declaration=True, type=unrelated)}
    assert entity_temperature_unit(config) == UNIT_CELSIUS


def test_a_platform_cannot_take_the_option_and_fix_the_unit() -> None:
    with pytest.raises(ValueError, match="not both"):
        climate_schema(
            Climate, temperature_unit=True, fixed_temperature_unit=UNIT_FAHRENHEIT
        )


def test_a_fixed_unit_is_not_passed_to_the_platform() -> None:
    calls: list[str] = []
    original_add = entity_helpers.add
    entity_helpers.add = lambda expression: calls.append(str(expression))
    try:
        var = MockObj("ac")
        fixed = climate_ns.class_("FixedUnitClimate", Climate)
        climate_schema(fixed, fixed_temperature_unit=UNIT_FAHRENHEIT)
        setup_entity_temperature_unit(
            var, {CONF_ID: ID("ac", is_declaration=True, type=fixed)}
        )
        assert calls == []
        setup_entity_temperature_unit(
            var, {CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit("°F")}
        )
        assert len(calls) == 1
        assert calls[0].startswith("ac.set_temperature_unit(")
        assert calls[0].endswith("TemperatureUnit::FAHRENHEIT)")
    finally:
        entity_helpers.add = original_add


def test_kelvin_is_rejected_with_mqtt() -> None:
    schema = climate_schema(Climate, temperature_unit=True)
    CORE.loaded_integrations.add("mqtt")
    try:
        with pytest.raises(cv.Invalid, match="°C or °F"):
            schema({CONF_NAME: "AC", CONF_UNIT_OF_MEASUREMENT: UNIT_KELVIN})
    finally:
        CORE.loaded_integrations.discard("mqtt")
    config = schema({CONF_NAME: "AC", CONF_UNIT_OF_MEASUREMENT: UNIT_KELVIN})
    assert config[CONF_UNIT_OF_MEASUREMENT] == UNIT_KELVIN


def test_action_temperatures_follow_the_target_entity_unit() -> None:
    ac = ID("ac", is_declaration=True, type="climate::Climate")
    full = Config()
    full["climate"] = [
        {CONF_ID: ac, CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT)}
    ]
    full.declare_ids.append((ac, ["climate", 0, CONF_ID]))
    CORE.config = full

    target = ID("ac", is_declaration=False)
    assert entity_temperature_unit_of_id(target) == UNIT_FAHRENHEIT
    assert temperature_in_entity_unit(
        {CONF_ID: target}, (20.0, UNIT_CELSIUS)
    ).startswith("68.0")


def test_a_resolved_unit_waits_for_the_full_config() -> None:
    resolved = climate_ns.class_("ResolvedUnitClimate", Climate)
    schema = climate_schema(
        resolved, fixed_temperature_unit=lambda config, full: UNIT_FAHRENHEIT
    )
    # The resolver needs the full config, so schema validation leaves the value as written
    config = schema({CONF_NAME: "AC", CONF_VISUAL: {CONF_MIN_TEMPERATURE: "15°C"}})
    assert entity_temperature_unit(config) is None
    assert config[CONF_VISUAL][CONF_MIN_TEMPERATURE] == (15.0, UNIT_CELSIUS)
    assert entity_temperature_unit(config, Config()) == UNIT_FAHRENHEIT
    _final_validate([config])
    assert config[CONF_VISUAL][CONF_MIN_TEMPERATURE] == pytest.approx(59.0)


@pytest.mark.parametrize(
    ("use_fahrenheit", "cn105_lines"),
    [
        (
            "true",
            (
                "cn105_ac->set_visual_min_temperature_override(60.0f);",
                "cn105_ac->set_visual_max_temperature_override(86.0f);",
                "apply_call.set_target_temperature(72.0f);",
            ),
        ),
        (
            "false",
            (
                "cn105_ac->set_visual_min_temperature_override(15.5555",
                "cn105_ac->set_visual_max_temperature_override(30.0f);",
                "apply_call.set_target_temperature(22.2222",
            ),
        ),
    ],
)
def test_platforms_reporting_in_fahrenheit_resolve_their_unit(
    use_fahrenheit: str,
    cn105_lines: tuple[str, ...],
    tmp_path: Path,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    # mitsubishi_cn105 follows its hub's use_fahrenheit; friedrich and islandaire are always °F
    source = component_config_path("fahrenheit_platforms.yaml").read_text(
        encoding="utf-8"
    )
    config_path = tmp_path / "fahrenheit_platforms.yaml"
    config_path.write_text(
        source.replace("${use_fahrenheit}", use_fahrenheit), encoding="utf-8"
    )
    main_cpp = generate_main(config_path)
    for line in cn105_lines:
        assert line in main_cpp
    assert "friedrich_ac->set_visual_min_temperature_override(59.0f);" in main_cpp
    assert "islandaire_ac->set_visual_min_temperature_override(59.0f);" in main_cpp
    # friedrich and islandaire are each sent 20°C as 68°F
    assert main_cpp.count("apply_call.set_target_temperature(68.0f);") == 2
