"""Check the status entity field tables against the datasheet."""

from esphome.components.bq25186 import binary_sensor, text_sensor

from ..hub_function_helpers import field_value, progmem_strings
from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS


def test_binary_sensor_fields_match_the_datasheet() -> None:
    assert [field.key for field in binary_sensor._FIELDS] == list(BINARY_SENSORS)
    for field in binary_sensor._FIELDS:
        key = field.key
        sheet_reg, bits, on_values = BINARY_SENSORS[key]
        assert field.register == sheet_reg, key
        for value in range(256):
            decoded = bool(
                (field.on_values >> ((value >> field.shift) & field.mask)) & 1
            )
            assert decoded == (field_value(value, bits) in on_values), (key, value)


def test_text_sensor_fields_match_the_datasheet() -> None:
    strings = progmem_strings("bq25186")
    assert [key for key, *_ in text_sensor._FIELDS] == list(TEXT_SENSORS)
    for key, field, first in text_sensor._FIELDS:
        sheet_reg, bits, states = TEXT_SENSORS[key]
        assert field.register == sheet_reg, key
        for value in range(256):
            decoded = strings[first + ((value >> field.shift) & field.mask)]
            assert decoded == states[field_value(value, bits)], (key, value)
