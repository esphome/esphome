"""Check the status entity field tables against the datasheet."""

from esphome.components.bq25186 import binary_sensor, text_sensor

from ..hub_function_helpers import field_value, progmem_strings
from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS


def test_binary_sensor_fields_match_the_datasheet() -> None:
    assert [field.key for field in binary_sensor._FIELDS] == list(BINARY_SENSORS)
    for field in binary_sensor._FIELDS:
        register, bits, on_values = BINARY_SENSORS[field.key]
        assert field.register == register, field.key
        for value in range(256):
            decoded = ((value >> field.shift) & field.mask) in field.on
            assert decoded == (field_value(value, bits) in on_values), (
                field.key,
                value,
            )


def test_text_sensor_fields_match_the_datasheet() -> None:
    strings = progmem_strings("bq25186")
    assert [field.key for field in text_sensor._FIELDS] == list(TEXT_SENSORS)
    for field in text_sensor._FIELDS:
        register, bits, states = TEXT_SENSORS[field.key]
        assert field.register == register, field.key
        for value in range(256):
            decoded = strings[field.first + ((value >> field.shift) & field.mask)]
            assert decoded == states[field_value(value, bits)], (field.key, value)
