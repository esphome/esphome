"""Tests for user-defined action field metadata (description / example)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import validate_variable
from esphome.config_validation import Invalid
from esphome.core import CORE

CONFIG = "tests/component_tests/api/test_action_metadata.yaml"
CONFIG_ESP8266 = "tests/component_tests/api/test_action_metadata_esp8266.yaml"
CONFIG_SHORTHAND = "tests/component_tests/api/test_action_metadata_shorthand.yaml"

# Table layout: name, arg names, description, arg descriptions, arg examples
PLAY_BUZZER_TABLE = (
    "static const char *const api_action0_strings[] PROGMEM = {"
    "api_action_str0, api_action_str1, api_action_str2, api_action_str3, "
    "api_action_str4, nullptr, api_action_str5, nullptr};"
)
# An action without metadata still carries the metadata slots (as nullptr)
PLAIN_ACTION_TABLE = (
    "static const char *const api_action1_strings[] PROGMEM = {"
    "api_action_str6, api_action_str7, nullptr, nullptr, nullptr};"
)


def test_metadata_is_emitted_as_progmem_table(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Every action string is a PROGMEM array referenced from one PROGMEM table."""
    main_cpp = generate_main(CONFIG)

    assert (
        'static constexpr char api_action_str0[] PROGMEM = "play_buzzer";' in main_cpp
    )
    assert (
        'static constexpr char api_action_str3[] PROGMEM = "Play an RTTTL melody on the buzzer";'
        in main_cpp
    )
    assert (
        'static constexpr char api_action_str5[] PROGMEM = "two_short:d=4,o=5,b=100:16e6,16e6";'
        in main_cpp
    )
    assert PLAY_BUZZER_TABLE in main_cpp
    assert PLAIN_ACTION_TABLE in main_cpp
    assert (
        "(api_action0_strings, fnv1_hash_extend(FNV1_OFFSET_BASIS, api_action_str0))"
        in main_cpp
    )
    defines = {d.name: d.value for d in CORE.defines}
    assert "USE_API_USER_DEFINED_ACTION_METADATA" in defines
    assert "API_USER_ACTION_STRINGS_SCRATCH_SIZE" not in defines


def test_esp8266_sizes_scratch_buffer_for_largest_action(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266 gets a scratch buffer define equal to the byte total of the largest action."""
    generate_main(CONFIG_ESP8266)

    defines = {d.name: d.value for d in CORE.defines}
    expected = sum(
        len(s)
        for s in (
            "play_buzzer",
            "song_str",
            "volume",
            "Play an RTTTL melody on the buzzer",
            "RTTTL melody string",
            "two_short:d=4,o=5,b=100:16e6,16e6",
        )
    )
    assert defines["API_USER_ACTION_STRINGS_SCRATCH_SIZE"].i == expected


def test_shorthand_variables_emit_no_metadata(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The name: type shorthand emits a name-only table and no define."""
    main_cpp = generate_main(CONFIG_SHORTHAND)

    assert (
        "static const char *const api_action0_strings[] PROGMEM = "
        "{api_action_str0, api_action_str1};" in main_cpp
    )
    assert "USE_API_USER_DEFINED_ACTION_METADATA" not in {d.name for d in CORE.defines}


def test_variable_shorthand_normalizes_to_mapping() -> None:
    """A bare type string validates to the mapping form."""
    assert validate_variable("string") == {"type": "string"}


def test_variable_mapping_requires_type() -> None:
    """The mapping form without type raises."""
    with pytest.raises(Invalid):
        validate_variable({"description": "no type given"})


def test_variable_rejects_unknown_keys() -> None:
    """Unknown keys in the mapping form raise."""
    with pytest.raises(Invalid):
        validate_variable({"type": "string", "selector": "text"})


def test_variable_rejects_invalid_type() -> None:
    """An unknown variable type raises in both forms."""
    with pytest.raises(Invalid):
        validate_variable("stringy")
    with pytest.raises(Invalid):
        validate_variable({"type": "stringy"})
