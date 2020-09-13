"""
Please Note:

These tests cover the process of identifying information about pins, they do not
check if the definition of MCUs and pins is correct.

"""
import logging

import pytest

from esphome.config_validation import Invalid
from esphome.core import EsphomeCore
from esphome import pins


MOCK_ESP8266_BOARD_ID = "_mock_esp8266"
MOCK_ESP8266_PINS = {'X0': 16, 'X1': 5, 'X2': 4, 'LED': 2}
MOCK_ESP8266_BOARD_ALIAS_ID = "_mock_esp8266_alias"
MOCK_ESP8266_FLASH_SIZE = pins.FLASH_SIZE_2_MB

MOCK_ESP32_BOARD_ID = "_mock_esp32"
MOCK_ESP32_PINS = {'Y0': 12, 'Y1': 8, 'Y2': 3, 'LED': 9, "A0": 8}
MOCK_ESP32_BOARD_ALIAS_ID = "_mock_esp32_alias"

UNKNOWN_PLATFORM = "STM32"


@pytest.fixture
def mock_mcu(monkeypatch):
    """
    Add a mock MCU into the lists as a stable fixture
    """
    pins.ESP8266_BOARD_PINS[MOCK_ESP8266_BOARD_ID] = MOCK_ESP8266_PINS
    pins.ESP8266_FLASH_SIZES[MOCK_ESP8266_BOARD_ID] = MOCK_ESP8266_FLASH_SIZE
    pins.ESP8266_BOARD_PINS[MOCK_ESP8266_BOARD_ALIAS_ID] = MOCK_ESP8266_BOARD_ID
    pins.ESP8266_FLASH_SIZES[MOCK_ESP8266_BOARD_ALIAS_ID] = MOCK_ESP8266_FLASH_SIZE
    pins.ESP32_BOARD_PINS[MOCK_ESP32_BOARD_ID] = MOCK_ESP32_PINS
    pins.ESP32_BOARD_PINS[MOCK_ESP32_BOARD_ALIAS_ID] = MOCK_ESP32_BOARD_ID
    yield
    del pins.ESP8266_BOARD_PINS[MOCK_ESP8266_BOARD_ID]
    del pins.ESP8266_FLASH_SIZES[MOCK_ESP8266_BOARD_ID]
    del pins.ESP8266_BOARD_PINS[MOCK_ESP8266_BOARD_ALIAS_ID]
    del pins.ESP8266_FLASH_SIZES[MOCK_ESP8266_BOARD_ALIAS_ID]
    del pins.ESP32_BOARD_PINS[MOCK_ESP32_BOARD_ID]
    del pins.ESP32_BOARD_PINS[MOCK_ESP32_BOARD_ALIAS_ID]


@pytest.fixture
def core(monkeypatch, mock_mcu):
    core = EsphomeCore()
    monkeypatch.setattr(pins, "CORE", core)
    return core


@pytest.fixture
def core_esp8266(core):
    core.esp_platform = "ESP8266"
    core.board = MOCK_ESP8266_BOARD_ID
    return core


@pytest.fixture
def core_esp32(core):
    core.esp_platform = "ESP32"
    core.board = MOCK_ESP32_BOARD_ID
    return core


class Test_lookup_pin:
    @pytest.mark.parametrize("value, expected", (
            ("X1", 5),
            ("MOSI", 13),
    ))
    def test_valid_esp8266_pin(self, core_esp8266, value, expected):
        actual = pins._lookup_pin(value)

        assert actual == expected

    def test_valid_esp8266_pin_alias(self, core_esp8266):
        core_esp8266.board = MOCK_ESP8266_BOARD_ALIAS_ID

        actual = pins._lookup_pin("X2")

        assert actual == 4

    @pytest.mark.parametrize("value, expected", (
            ("Y1", 8),
            ("A0", 8),
            ("MOSI", 23),
    ))
    def test_valid_esp32_pin(self, core_esp32, value, expected):
        actual = pins._lookup_pin(value)

        assert actual == expected

    def test_valid_32_pin_alias(self, core_esp32):
        core_esp32.board = MOCK_ESP32_BOARD_ALIAS_ID

        actual = pins._lookup_pin("Y2")

        assert actual == 3

    def test_invalid_pin(self, core_esp8266):
        with pytest.raises(Invalid, match="Cannot resolve pin name 'X42' for board _mock_esp8266."):
            pins._lookup_pin("X42")

    def test_unsupported_platform(self, core):
        core.esp_platform = UNKNOWN_PLATFORM

        with pytest.raises(NotImplementedError):
            pins._lookup_pin("TX")


class Test_translate_pin:
    @pytest.mark.parametrize("value, expected", (
            (2, 2),
            ("3", 3),
            ("GPIO4", 4),
            ("TX", 1),
            ("Y0", 12),
    ))
    def test_valid_values(self, core_esp32, value, expected):
        actual = pins._translate_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value", ({}, None))
    def test_invalid_values(self, core_esp32, value):
        with pytest.raises(Invalid, match="This variable only supports"):
            pins._translate_pin(value)


class Test_validate_gpio_pin:
    def test_esp32_valid(self, core_esp32):
        actual = pins.validate_gpio_pin("GPIO22")

        assert actual == 22

    @pytest.mark.parametrize("value, match", (
            (-1, "ESP32: Invalid pin number: -1"),
            (40, "ESP32: Invalid pin number: 40"),
            (6, "This pin cannot be used on ESP32s and"),
            (7, "This pin cannot be used on ESP32s and"),
            (8, "This pin cannot be used on ESP32s and"),
            (11, "This pin cannot be used on ESP32s and"),
            (20, "The pin GPIO20 is not usable on ESP32s"),
            (24, "The pin GPIO24 is not usable on ESP32s"),
            (28, "The pin GPIO28 is not usable on ESP32s"),
            (29, "The pin GPIO29 is not usable on ESP32s"),
            (30, "The pin GPIO30 is not usable on ESP32s"),
            (31, "The pin GPIO31 is not usable on ESP32s"),
    ))
    def test_esp32_invalid_pin(self, core_esp32, value, match):
        with pytest.raises(Invalid, match=match):
            pins.validate_gpio_pin(value)

    @pytest.mark.parametrize("value", (9, 10))
    def test_esp32_warning(self, core_esp32, caplog, value):
        caplog.at_level(logging.WARNING)
        pins.validate_gpio_pin(value)

        assert len(caplog.messages) == 1
        assert caplog.messages[0].endswith("flash interface in QUAD IO flash mode.")

    def test_esp8266_valid(self, core_esp8266):
        actual = pins.validate_gpio_pin("GPIO12")

        assert actual == 12

    @pytest.mark.parametrize("value, match", (
            (-1, "ESP8266: Invalid pin number: -1"),
            (18, "ESP8266: Invalid pin number: 18"),
            (6, "This pin cannot be used on ESP8266s and"),
            (7, "This pin cannot be used on ESP8266s and"),
            (8, "This pin cannot be used on ESP8266s and"),
            (11, "This pin cannot be used on ESP8266s and"),
    ))
    def test_esp8266_invalid_pin(self, core_esp8266, value, match):
        with pytest.raises(Invalid, match=match):
            pins.validate_gpio_pin(value)

    @pytest.mark.parametrize("value", (9, 10))
    def test_esp8266_warning(self, core_esp8266, caplog, value):
        caplog.at_level(logging.WARNING)
        pins.validate_gpio_pin(value)

        assert len(caplog.messages) == 1
        assert caplog.messages[0].endswith("flash interface in QUAD IO flash mode.")

    def test_unknown_device(self, core):
        core.esp_platform = UNKNOWN_PLATFORM

        with pytest.raises(NotImplementedError):
            pins.validate_gpio_pin("0")


class Test_input_pin:
    @pytest.mark.parametrize("value, expected", (
            ("X0", 16),
    ))
    def test_valid_esp8266_values(self, core_esp8266, value, expected):
        actual = pins.input_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value, expected", (
            ("Y0", 12),
            (17, 17),
    ))
    def test_valid_esp32_values(self, core_esp32, value, expected):
        actual = pins.input_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value", (17,))
    def test_invalid_esp8266_values(self, core_esp8266, value):
        with pytest.raises(Invalid):
            pins.input_pin(value)

    def test_unknown_platform(self, core):
        core.esp_platform = UNKNOWN_PLATFORM

        with pytest.raises(NotImplementedError):
            pins.input_pin(2)


class Test_input_pullup_pin:
    @pytest.mark.parametrize("value, expected", (
            ("X0", 16),
    ))
    def test_valid_esp8266_values(self, core_esp8266, value, expected):
        actual = pins.input_pullup_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value, expected", (
            ("Y0", 12),
            (17, 17),
    ))
    def test_valid_esp32_values(self, core_esp32, value, expected):
        actual = pins.input_pullup_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value", (0,))
    def test_invalid_esp8266_values(self, core_esp8266, value):
        with pytest.raises(Invalid):
            pins.input_pullup_pin(value)

    def test_unknown_platform(self, core):
        core.esp_platform = UNKNOWN_PLATFORM

        with pytest.raises(NotImplementedError):
            pins.input_pullup_pin(2)


class Test_output_pin:
    @pytest.mark.parametrize("value, expected", (
            ("X0", 16),
    ))
    def test_valid_esp8266_values(self, core_esp8266, value, expected):
        actual = pins.output_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value, expected", (
            ("Y0", 12),
            (17, 17),
    ))
    def test_valid_esp32_values(self, core_esp32, value, expected):
        actual = pins.output_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value", (17,))
    def test_invalid_esp8266_values(self, core_esp8266, value):
        with pytest.raises(Invalid):
            pins.output_pin(value)

    @pytest.mark.parametrize("value", range(34, 40))
    def test_invalid_esp32_values(self, core_esp32, value):
        with pytest.raises(Invalid):
            pins.output_pin(value)

    def test_unknown_platform(self, core):
        core.esp_platform = UNKNOWN_PLATFORM

        with pytest.raises(NotImplementedError):
            pins.output_pin(2)


class Test_analog_pin:
    @pytest.mark.parametrize("value, expected", (
            (17, 17),
    ))
    def test_valid_esp8266_values(self, core_esp8266, value, expected):
        actual = pins.analog_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value, expected", (
            (32, 32),
            (39, 39),
    ))
    def test_valid_esp32_values(self, core_esp32, value, expected):
        actual = pins.analog_pin(value)

        assert actual == expected

    @pytest.mark.parametrize("value", ("X0",))
    def test_invalid_esp8266_values(self, core_esp8266, value):
        with pytest.raises(Invalid):
            pins.analog_pin(value)

    @pytest.mark.parametrize("value", ("Y0",))
    def test_invalid_esp32_values(self, core_esp32, value):
        with pytest.raises(Invalid):
            pins.analog_pin(value)

    def test_unknown_platform(self, core):
        core.esp_platform = UNKNOWN_PLATFORM

        with pytest.raises(NotImplementedError):
            pins.analog_pin(2)
