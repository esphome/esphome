"""Tests for the Home Assistant action field tables."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import DATA_FIELDS_SCHEMA, VARIABLES_FIELDS_SCHEMA
import esphome.config_validation as cv

CONFIG = "tests/component_tests/api/test_homeassistant_fields.yaml"


def test_field_tables(generate_main: Callable[[str | Path], str]) -> None:
    """Strings are single PROGMEM arrays on ESP8266; only tables without lambdas are shared."""
    main_cpp = generate_main(CONFIG)

    assert main_cpp.count('PROGMEM = "message";') == 1
    assert main_cpp.count('PROGMEM = "notify.notify";') == 1
    assert 'ESPHOME_F("message")' not in main_cpp

    assert main_cpp.count("(api_apiserver_id, false, ha_action_fields, 1, 0, 0);") == 2

    assert "(api_apiserver_id, true, ha_action_fields_2, 1, 0, 0);" in main_cpp
    assert "(api_apiserver_id, true, ha_action_fields_3, 1, 0, 0);" in main_cpp


@pytest.mark.parametrize("schema", [DATA_FIELDS_SCHEMA, VARIABLES_FIELDS_SCHEMA])
def test_field_map_limit(schema: cv.Schema) -> None:
    """Each map is counted in a uint8_t, so 256 entries fail validation."""
    schema({f"key{i}": "value" for i in range(255)})
    with pytest.raises(cv.Invalid):
        schema({f"key{i}": "value" for i in range(256)})
