"""Tests for the hlk_fm22x component."""

import logging

import pytest

from esphome import config_validation as cv
from esphome.components import hlk_fm22x


def test_hlk_fm22x_action_arguments(generate_main) -> None:
    """Action options reach the component in the order its methods take them."""
    main_cpp = generate_main("tests/component_tests/hlk_fm22x/test_hlk_fm22x.yaml")

    # A numeric and a named direction, and the defaults used by the shorthand form
    enroll = "face_module->enroll_face("
    direction = "static_cast<hlk_fm22x::HlkFm22xFaceDirection>"
    assert f'{enroll}"Test", {direction}(1), false, 10, true);' in main_cpp
    assert (
        f'{enroll}"Admin", {direction}(hlk_fm22x::FACE_DIRECTION_LEFT), true, 20, false);'
        in main_cpp
    )
    # Without a direction the face is enrolled from a single frame
    assert f'{enroll}"Single", {direction}(0), false, 10, true);' in main_cpp
    # Timeouts are passed in seconds
    assert "->scan_face(15);" in main_cpp


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
    """Names are printable ASCII and fit the module's 32 byte field with room for the terminator."""
    assert hlk_fm22x.validate_name("a" * 31) == "a" * 31
    assert hlk_fm22x.validate_name("Jonny D") == "Jonny D"
    with pytest.raises(cv.Invalid):
        hlk_fm22x.validate_name("a" * 32)
    # A match reply is trimmed to printable ASCII, so other characters would come back mangled
    with pytest.raises(cv.Invalid):
        hlk_fm22x.validate_name("Jos\u00e9")
    with pytest.raises(cv.Invalid):
        hlk_fm22x.validate_name("tab\there")
