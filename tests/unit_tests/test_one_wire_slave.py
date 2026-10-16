import pytest

from esphome.components.one_wire_slave import (
    encode_scratchpad,
    validate_rom_address,
    validate_timing_scale,
)


def test_ds18b20_rom_address_has_family_and_valid_crc() -> None:
    address = 0x9E06050403020128

    result = validate_rom_address(address)

    assert result == address


def test_rom_address_rejects_non_ds18b20_family() -> None:
    with pytest.raises(ValueError, match="family code 0x28"):
        validate_rom_address(0x0001020304050622)


def test_rom_address_rejects_invalid_crc() -> None:
    with pytest.raises(ValueError, match="CRC"):
        validate_rom_address(0x9901020304050628)


def test_timing_scale_accepts_slow_profile() -> None:
    assert validate_timing_scale(20) == 20


def test_timing_scale_rejects_invalid_values() -> None:
    with pytest.raises(ValueError, match="between 1 and 100"):
        validate_timing_scale(0)


def test_scratchpad_encodes_positive_temperature_and_crc() -> None:
    scratchpad = encode_scratchpad(23.5, 12, 75, 70)

    assert scratchpad[:5] == bytes((0x78, 0x01, 75, 70, 0x7F))
    assert scratchpad[5:8] == bytes((0xFF, 0x0C, 0x10))
    assert len(scratchpad) == 9


def test_scratchpad_quantizes_negative_temperature_at_nine_bits() -> None:
    scratchpad = encode_scratchpad(-10.125, 9, 75, 70)

    assert scratchpad[0:2] == bytes((0x58, 0xFF))
    assert scratchpad[4] == 0x1F
