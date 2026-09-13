"""Tests for the hlk_fm22x component."""

import logging
import re

import pytest

from esphome import config_validation as cv
from esphome.components import hlk_fm22x


def _returned_values(main_cpp: str, setter: str, cpp_type: str) -> list[str]:
    """Collect the constants returned by the lambdas passed to a templatable setter."""
    pattern = rf"{setter}\(\[\]\(\) -> {cpp_type} \{{\s*return ([^;]+);"
    return [match.strip() for match in re.findall(pattern, main_cpp)]


def test_hlk_fm22x_entities_and_actions(generate_main) -> None:
    """Every entity type and action option ends up in the generated code."""
    main_cpp = generate_main("tests/component_tests/hlk_fm22x/test_hlk_fm22x.yaml")

    assert "new(face_module) hlk_fm22x::HlkFm22xComponent();" in main_cpp
    # Plain component now, no polling
    assert "face_module->set_update_interval(" not in main_cpp
    assert "set_enrolling_binary_sensor(" in main_cpp
    assert "set_scanning_binary_sensor(" in main_cpp
    assert "set_face_count_sensor(" in main_cpp
    assert "set_version_text_sensor(" in main_cpp
    assert "set_serial_number_text_sensor(" in main_cpp
    assert "set_face_state_text_sensor(" in main_cpp

    # Templatable values are emitted as small lambdas returning the constant.
    # Enrollment options, including a numeric and a named direction
    directions = _returned_values(main_cpp, "set_direction", "uint8_t")
    assert "1" in directions
    assert any("FACE_DIRECTION_LEFT" in value for value in directions)
    assert "true" in _returned_values(main_cpp, "set_admin", "bool")
    assert "false" in _returned_values(main_cpp, "set_allow_duplicate", "bool")
    assert "set_timeout_seconds(20)" in main_cpp
    # A scan with its own timeout and the default one used by the shorthand enroll
    assert "set_timeout_seconds(15)" in main_cpp
    assert "set_timeout_seconds(10)" in main_cpp

    assert "hlk_fm22x::CancelAction<>" in main_cpp
    assert "hlk_fm22x::GetFaceDetailsAction<>" in main_cpp
    assert "3" in _returned_values(main_cpp, "set_face_id", "int16_t")


def test_hlk_fm22x_legacy_config(
    generate_main, caplog: pytest.LogCaptureFixture
) -> None:
    """Deprecated forms still generate working code and warn about the change."""
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(
            "tests/component_tests/hlk_fm22x/test_hlk_fm22x_legacy.yaml"
        )

    assert "face_module->set_enrolling_binary_sensor(" in main_cpp
    assert "face_module->set_update_interval(" not in main_cpp

    messages = [record.message for record in caplog.records]
    assert any("'update_interval' is no longer used" in m for m in messages)
    assert any("move its options under 'enrolling:'" in m for m in messages)


def test_direction_validator() -> None:
    """Directions can be given as the module's codes or as names."""
    assert hlk_fm22x.validate_direction(4) == 4
    # Any byte value is passed to the module unchanged, 0 means single frame
    assert hlk_fm22x.validate_direction(0) == 0
    left = hlk_fm22x.validate_direction("Left")
    assert left.enum_value is hlk_fm22x.FACE_DIRECTIONS["left"]
    with pytest.raises(cv.Invalid):
        hlk_fm22x.validate_direction(256)
    with pytest.raises(cv.Invalid):
        hlk_fm22x.validate_direction("sideways")


def test_name_validator() -> None:
    """Names must fit the module's 32 byte field with room for the terminator."""
    assert hlk_fm22x.validate_name("a" * 31) == "a" * 31
    with pytest.raises(cv.Invalid):
        hlk_fm22x.validate_name("a" * 32)
    # Multi byte characters count by their encoded size
    with pytest.raises(cv.Invalid):
        hlk_fm22x.validate_name("\u00e9" * 16)


def test_timeout_schema() -> None:
    """The module accepts timeouts from 1 to 255 seconds."""
    assert hlk_fm22x.TIMEOUT_SCHEMA("20s").total_seconds == 20
    assert hlk_fm22x.TIMEOUT_SCHEMA("2min").total_seconds == 120
    with pytest.raises(cv.Invalid):
        hlk_fm22x.TIMEOUT_SCHEMA("0s")
    with pytest.raises(cv.Invalid):
        hlk_fm22x.TIMEOUT_SCHEMA("300s")
