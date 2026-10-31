import pytest

from esphome.components.one_wire_device import validate_rom_address


def test_ds18b20_rom_address_has_family_and_valid_crc() -> None:
    assert validate_rom_address(0x9E06050403020128) == 0x9E06050403020128


def test_rom_address_rejects_non_ds18b20_family() -> None:
    with pytest.raises(ValueError, match="family code"):
        validate_rom_address(0x0001020304050622)


def test_rom_address_rejects_invalid_crc() -> None:
    with pytest.raises(ValueError, match="CRC"):
        validate_rom_address(0x9901020304050628)
