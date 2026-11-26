"""Tests for user-defined action field metadata (description / example)."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.api import (
    _action_strings,
    _has_action_metadata,
    validate_variable,
)
from esphome.config_validation import Invalid
from esphome.core import CORE
from esphome.cpp_generator import safe_exp
from esphome.helpers import fnv1_hash

CONFIG = "tests/component_tests/api/test_action_metadata.yaml"
CONFIG_ESP8266 = "tests/component_tests/api/test_action_metadata_esp8266.yaml"
CONFIG_SHORTHAND = "tests/component_tests/api/test_action_metadata_shorthand.yaml"


def test_metadata_is_emitted_as_progmem_table(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Every action string is a literal referenced from one PROGMEM table."""
    main_cpp = generate_main(CONFIG)

    assert (
        "static constexpr const char * api_action0_strings[] PROGMEM = {"
        '"play_buzzer", "Play an RTTTL melody on the buzzer", "song_str", '
        '"RTTTL melody string", "two_short:d=4,o=5,b=100:16e6,16e6", "volume", '
        "nullptr, nullptr};" in main_cpp
    )
    # An action without metadata still carries the metadata slots (as nullptr)
    assert (
        "static constexpr const char * api_action1_strings[] PROGMEM = {"
        '"plain_action", nullptr, "value", nullptr, nullptr};' in main_cpp
    )
    assert f"(api_action0_strings, {safe_exp(fnv1_hash('play_buzzer'))});" in main_cpp
    assert "USE_API_USER_DEFINED_ACTION_METADATA" in {d.name for d in CORE.defines}


def test_esp8266_strings_are_shared_progmem_arrays(
    generate_main: Callable[[str | Path], str],
) -> None:
    """On ESP8266 each string is its own PROGMEM array, referenced from the table."""
    main_cpp = generate_main(CONFIG_ESP8266)

    def array_for(text: str) -> str:
        match = re.search(
            rf'static constexpr char (progmem_str\w*)\[\] PROGMEM = "{text}";', main_cpp
        )
        assert match, text
        return match.group(1)

    assert array_for("play_buzzer")
    assert (
        "static constexpr const char * api_action1_strings[] PROGMEM = {"
        f"{array_for('plain_action')}, nullptr, {array_for('value')}, nullptr, nullptr}};"
        in main_cpp
    )


def test_esp8266_encodes_action_strings_from_flash(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266 uses the same flash string table."""
    main_cpp = generate_main(CONFIG_ESP8266)

    assert "api_action0_strings[] PROGMEM" in main_cpp


def test_shorthand_variables_emit_no_metadata(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The name: type shorthand emits a name-only table and no define."""
    main_cpp = generate_main(CONFIG_SHORTHAND)

    assert (
        "static constexpr const char * api_action0_strings[] PROGMEM = "
        '{"plain_action", "value"};' in main_cpp
    )
    assert "USE_API_USER_DEFINED_ACTION_METADATA" not in {d.name for d in CORE.defines}


def test_variable_shorthand_normalizes_to_mapping() -> None:
    """A bare type string validates to the mapping form."""
    assert validate_variable("string") == {"type": "string"}


@pytest.mark.parametrize(
    "value",
    [
        {"description": "no type given"},
        {"type": "string", "selector": "text"},
        "stringy",
        {"type": "stringy"},
    ],
)
def test_variable_rejects_invalid(value: object) -> None:
    """Missing or unknown type and unknown keys raise in both forms."""
    with pytest.raises(Invalid):
        validate_variable(value)


def test_empty_metadata_is_unset() -> None:
    """An empty description or example emits nullptr."""
    conf = {
        "action": "a",
        "description": "",
        "variables": {"b": {"type": "int", "description": "", "example": "ex"}},
    }
    strings = _action_strings(conf, has_metadata=True, has_optional=False)
    assert strings == ["a", None, "b", None, "ex"]


def test_empty_metadata_does_not_enable_the_define() -> None:
    actions = [
        {
            "action": "a",
            "description": "",
            "variables": {"b": {"type": "int", "example": ""}},
        }
    ]
    assert not _has_action_metadata(actions)
    actions[0]["variables"]["b"]["example"] = "1"
    assert _has_action_metadata(actions)
