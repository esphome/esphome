"""Tests for user-defined action field metadata (description / example)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import (
    _action_strings,
    _action_strings_size,
    _has_action_metadata,
    _validate_esp8266_action_strings,
    validate_variable,
)
from esphome.config_validation import Invalid
from esphome.const import PlatformFramework
from esphome.core import CORE
from esphome.cpp_generator import safe_exp
from esphome.helpers import fnv1_hash
from tests.component_tests.helpers import get_define_value
from tests.component_tests.types import SetCoreConfigCallable

CONFIG = "tests/component_tests/api/test_action_metadata.yaml"
CONFIG_ESP8266 = "tests/component_tests/api/test_action_metadata_esp8266.yaml"
CONFIG_SHORTHAND = "tests/component_tests/api/test_action_metadata_shorthand.yaml"


def test_metadata_is_emitted_as_progmem_table(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Every action string is a PROGMEM array referenced from one PROGMEM table."""
    main_cpp = generate_main(CONFIG)

    assert (
        'static constexpr char api_action_str0[] PROGMEM = "play_buzzer";' in main_cpp
    )
    assert (
        'static constexpr char api_action_str1[] PROGMEM = "Play an RTTTL melody on the buzzer";'
        in main_cpp
    )
    assert (
        'static constexpr char api_action_str4[] PROGMEM = "two_short:d=4,o=5,b=100:16e6,16e6";'
        in main_cpp
    )
    assert (
        "static constexpr const char * api_action0_strings[] PROGMEM = {"
        "api_action_str0, api_action_str1, api_action_str2, api_action_str3, "
        "api_action_str4, api_action_str5, nullptr, nullptr};" in main_cpp
    )
    # An action without metadata still carries the metadata slots (as nullptr)
    assert (
        "static constexpr const char * api_action1_strings[] PROGMEM = {"
        "api_action_str6, nullptr, api_action_str7, nullptr, nullptr};" in main_cpp
    )
    assert f"(api_action0_strings, {safe_exp(fnv1_hash('play_buzzer'))});" in main_cpp
    assert "USE_API_USER_DEFINED_ACTION_METADATA" in {d.name for d in CORE.defines}
    assert get_define_value("API_USER_ACTION_STRINGS_SCRATCH_SIZE") is None


def test_esp8266_sizes_scratch_buffer_for_largest_action(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266 gets a scratch buffer define equal to the byte total of the largest action."""
    generate_main(CONFIG_ESP8266)

    # play_buzzer: name, description, two variable names, one description, one example,
    # each with a terminator
    assert get_define_value("API_USER_ACTION_STRINGS_SCRATCH_SIZE") == "117"


def test_shorthand_variables_emit_no_metadata(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The name: type shorthand emits a name-only table and no define."""
    main_cpp = generate_main(CONFIG_SHORTHAND)

    assert (
        "static constexpr const char * api_action0_strings[] PROGMEM = "
        "{api_action_str0, api_action_str1};" in main_cpp
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


def _oversized_action_config() -> dict:
    return {
        "actions": [
            {
                "action": "big",
                "description": "x" * 300,
                "variables": {"a": {"type": "string", "example": "y" * 300}},
            }
        ]
    }


def test_esp8266_rejects_actions_over_string_budget(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(Invalid, match="ESP8266 allows at most 384 bytes"):
        _validate_esp8266_action_strings(_oversized_action_config())


def test_other_platforms_have_no_string_budget(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = _oversized_action_config()
    assert _validate_esp8266_action_strings(config) is config


def test_empty_metadata_is_unset_and_not_counted() -> None:
    """An empty description or example emits nullptr and takes no scratch space."""
    conf = {
        "action": "a",
        "description": "",
        "variables": {"b": {"type": "int", "description": "", "example": "ex"}},
    }
    strings = _action_strings(conf, has_metadata=True)
    assert strings == ["a", None, "b", None, "ex"]
    # Every emitted string counts its terminator: "a" + "b" + "ex"
    assert _action_strings_size(strings) == 2 + 2 + 3


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
