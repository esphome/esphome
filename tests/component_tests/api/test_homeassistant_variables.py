"""Tests for variables handling in homeassistant.event and homeassistant.action."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

CONFIG = "tests/component_tests/api/test_homeassistant_variables.yaml"


def test_plain_string_with_return_is_compiled_as_lambda_with_warning(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A plain string with a return statement compiles as a lambda and warns."""
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(CONFIG)

    assert main_cpp.count('add_variable(ESPHOME_F("lambda_var"), []() {') == 2
    assert "return millis();" in main_cpp
    # The source text must not be sent as a static string value.
    assert '"return millis();"' not in main_cpp
    assert "missing the !lambda tag" in caplog.text


def test_static_string_is_kept_as_static_value(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A static string stays static, PROGMEM wrapped, with no warning."""
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(CONFIG)

    assert (
        main_cpp.count(
            'add_variable(ESPHOME_F("static_var"), ESPHOME_F("static value"));'
        )
        == 2
    )
    assert "static value" not in caplog.text


def test_static_id_value_stays_literal_with_hint(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Lambda source without a return stays literal text but warns."""
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(CONFIG)

    assert 'ESPHOME_F("id(test_sensor).state")' in main_cpp
    assert "sent as literal text" in caplog.text


def test_explicit_lambda_tag_is_compiled_as_lambda(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A !lambda value keeps working unchanged."""
    main_cpp = generate_main(CONFIG)

    assert 'add_variable(ESPHOME_F("tagged_var"), []() {' in main_cpp
    assert "return App.get_name();" in main_cpp
