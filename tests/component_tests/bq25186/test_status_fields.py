"""Check the status entity field tables against the datasheet."""

from esphome.components.bq25186 import binary_sensor, text_sensor

from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS, field_value, status_strings


def test_binary_sensor_fields_match_the_datasheet() -> None:
    assert [key for key, *_ in binary_sensor._FIELDS] == list(BINARY_SENSORS)
    for key, reg, shift, mask, match, _ in binary_sensor._FIELDS:
        sheet_reg, bits, on_values = BINARY_SENSORS[key]
        assert reg == sheet_reg, key
        for value in range(256):
            decoded = bool((match >> ((value >> shift) & mask)) & 1)
            assert decoded == (field_value(value, bits) in on_values), (key, value)


def test_text_sensor_fields_match_the_datasheet() -> None:
    strings = status_strings()
    assert [key for key, *_ in text_sensor._FIELDS] == list(TEXT_SENSORS)
    for key, reg, shift, first in text_sensor._FIELDS:
        sheet_reg, bits, states = TEXT_SENSORS[key]
        assert reg == sheet_reg, key
        for value in range(256):
            decoded = strings[first + ((value >> shift) & 0x03)]
            assert decoded == states[field_value(value, bits)], (key, value)
