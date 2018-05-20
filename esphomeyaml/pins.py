import logging

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import core
from esphomeyaml.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_PCF8574, \
    ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266

_LOGGER = logging.getLogger(__name__)

ESP8266_PINS = {
    'A0': 17, 'SS': 15, 'MOSI': 13, 'MISO': 12, 'SCK': 14,
}
ESP8266_NODEMCU_PINS = dict(ESP8266_PINS, **{
    'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13, 'D8': 15, 'D9': 3,
    'D10': 1, 'LED': 16, 'SDA': 4, 'SCL': 5,
})
ESP8266_D1_PINS = dict(ESP8266_PINS, **{
    'D0': 3, 'D1': 1, 'D2': 16, 'D3': 5, 'D4': 4, 'D5': 14, 'D6': 12, 'D7': 13, 'D8': 0, 'D9': 2,
    'D10': 15, 'D11': 13, 'D12': 14, 'D13': 14, 'D14': 4, 'D15': 5, 'LED': 2, 'SDA': 4, 'SCL': 5,
})
ESP8266_D1_MINI_PINS = dict(ESP8266_PINS, **{
    'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13, 'D8': 15, 'RX': 3,
    'TX': 1, 'LED': 2, 'SDA': 4, 'SCL': 5,
})
ESP8266_THING_PINS = dict(ESP8266_PINS, **{
    'LED': 5, 'SDA': 2, 'SCL': 14,
})
ESP8266_ADAFRUIT_PINS = dict(ESP8266_PINS, **{
    'LED': 0, 'SDA': 4, 'SCL': 5,
})
ESP8266_ESPDUINO_PINS = dict(ESP8266_PINS, **{
    'LED': 16, 'SDA': 4, 'SCL': 5,
})
ESP8266_BOARD_TO_PINS = {
    'huzzah': ESP8266_ADAFRUIT_PINS,
    'espduino': ESP8266_ESPDUINO_PINS,
    'nodemcu': ESP8266_NODEMCU_PINS, 'nodemcuv2': ESP8266_NODEMCU_PINS,
    'thing': ESP8266_THING_PINS, 'thingdev': ESP8266_THING_PINS,
    'd1': ESP8266_D1_PINS,
    'd1_mini': ESP8266_D1_MINI_PINS, 'd1_mini_lite': ESP8266_D1_MINI_PINS,
    'd1_mini_pro': ESP8266_D1_MINI_PINS
}

ESP32_PINS = {
    'TX': 1, 'RX': 3, 'SDA': 21, 'SCL': 22, 'SS': 5, 'MOSI': 23, 'MISO': 19, 'SCK': 18, 'A0': 36,
    'A3': 39, 'A4': 32, 'A5': 33, 'A6': 34, 'A7': 35, 'A10': 4, 'A11': 0, 'A12': 2, 'A13': 15,
    'A14': 13, 'A15': 12, 'A16': 14, 'A17': 27, 'A18': 25, 'A19': 26, 'T0': 4, 'T1': 0, 'T2': 2,
    'T3': 15, 'T4': 12, 'T5': 12, 'T6': 14, 'T7': 27, 'T8': 33, 'T9': 32, 'DAC1': 25, 'DAC2': 26,
    'SVP': 36, 'SVN': 39,
}
ESP32_NODEMCU_32S_PINS = dict(ESP32_PINS, **{
    'LED': 2,
})
ESP32_LOLIN32_PINS = dict(ESP32_PINS, **{
    'LED': 5
})
ESP32_BOARD_TO_PINS = {
    'nodemcu-32s': ESP32_NODEMCU_32S_PINS,
    'lolin32': ESP32_LOLIN32_PINS,
}


def _translate_pin(value):
    if isinstance(value, dict) or value is None:
        raise vol.Invalid(u"This variable only supports pin numbers, not full pin schemas "
                          u"(with inverted and mode).")
    if isinstance(value, int):
        return value
    try:
        return int(value)
    except ValueError:
        pass
    if value.startswith('GPIO'):
        return vol.Coerce(int)(value[len('GPIO'):].strip())
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        if value in ESP32_PINS:
            return ESP32_PINS[value]
        if core.BOARD not in ESP32_BOARD_TO_PINS:
            raise vol.Invalid(u"ESP32: Unknown board {} with unknown "
                              u"pin {}.".format(core.BOARD, value))
        if value not in ESP32_BOARD_TO_PINS[core.BOARD]:
            raise vol.Invalid(u"ESP32: Board {} doesn't have "
                              u"pin {}".format(core.BOARD, value))
        return ESP32_BOARD_TO_PINS[core.BOARD][value]
    elif core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        if value in ESP8266_PINS:
            return ESP8266_PINS[value]
        if core.BOARD not in ESP8266_BOARD_TO_PINS:
            raise vol.Invalid(u"ESP8266: Unknown board {} with unknown "
                              u"pin {}.".format(core.BOARD, value))
        if value not in ESP8266_BOARD_TO_PINS[core.BOARD]:
            raise vol.Invalid(u"ESP8266: Board {} doesn't have "
                              u"pin {}".format(core.BOARD, value))
        return ESP8266_BOARD_TO_PINS[core.BOARD][value]
    raise vol.Invalid(u"Invalid ESP platform.")


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        if value < 0 or value > 39:
            raise vol.Invalid(u"ESP32: Invalid pin number: {}".format(value))
        if 6 <= value <= 11:
            _LOGGER.warning(u"ESP32: Pin %s (6-11) might already be used by the "
                            u"flash interface. Be warned.", value)
        if value in (20, 24, 28, 29, 30, 31):
            _LOGGER.warning(u"ESP32: Pin %s (20, 24, 28-31) can usually not be used. "
                            u"Be warned.", value)
        return value
    elif core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        if 6 <= value <= 11:
            _LOGGER.warning(u"ESP8266: Pin %s (6-11) might already be used by the "
                            u"flash interface. Be warned.", value)
        if value < 0 or value > 17:
            raise vol.Invalid(u"ESP8266: Invalid pin number: {}".format(value))
        return value
    raise vol.Invalid(u"Invalid ESP platform.")


def input_pin(value):
    value = validate_gpio_pin(value)
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        return value
    elif core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        return value
    raise vol.Invalid(u"Invalid ESP platform.")


def output_pin(value):
    value = validate_gpio_pin(value)
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        if 34 <= value <= 39:
            raise vol.Invalid(u"ESP32: Pin {} (34-39) can only be used as "
                              u"input pins.".format(value))
        return value
    elif core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        return value
    raise vol.Invalid("Invalid ESP platform.")


def analog_pin(value):
    value = validate_gpio_pin(value)
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        if 32 <= value <= 39:  # ADC1
            return value
        raise vol.Invalid(u"ESP32: Only pins 32 though 39 support ADC.")
    elif core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        if value == 17:  # A0
            return value
        raise vol.Invalid(u"ESP8266: Only pin A0 (17) supports ADC.")
    raise vol.Invalid(u"Invalid ESP platform.")


# pylint: disable=invalid-name
input_output_pin = vol.All(input_pin, output_pin)
gpio_pin = vol.Any(input_pin, output_pin)
PIN_MODES_ESP8266 = [
    'INPUT', 'OUTPUT', 'INPUT_PULLUP', 'OUTPUT_OPEN_DRAIN', 'SPECIAL', 'FUNCTION_1',
    'FUNCTION_2', 'FUNCTION_3', 'FUNCTION_4',
    'FUNCTION_0', 'WAKEUP_PULLUP', 'WAKEUP_PULLDOWN', 'INPUT_PULLDOWN_16',
]
PIN_MODES_ESP32 = [
    'INPUT', 'OUTPUT', 'INPUT_PULLUP', 'OUTPUT_OPEN_DRAIN', 'SPECIAL', 'FUNCTION_1',
    'FUNCTION_2', 'FUNCTION_3', 'FUNCTION_4',
    'PULLUP', 'PULLDOWN', 'INPUT_PULLDOWN', 'OPEN_DRAIN', 'FUNCTION_5',
    'FUNCTION_6', 'ANALOG',
]


def pin_mode(value):
    value = vol.All(vol.Coerce(str), vol.Upper)(value)
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        return cv.one_of(*PIN_MODES_ESP32)(value)
    elif core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        return cv.one_of(*PIN_MODES_ESP8266)(value)
    raise vol.Invalid(u"Invalid ESP platform.")


GPIO_FULL_OUTPUT_PIN_SCHEMA = vol.Schema({
    vol.Required(CONF_NUMBER): output_pin,
    vol.Optional(CONF_MODE): pin_mode,
    vol.Optional(CONF_INVERTED): cv.boolean,
})

GPIO_FULL_INPUT_PIN_SCHEMA = vol.Schema({
    vol.Required(CONF_NUMBER): output_pin,
    vol.Optional(CONF_MODE): pin_mode,
    vol.Optional(CONF_INVERTED): cv.boolean,
})


def shorthand_output_pin(value):
    value = output_pin(value)
    return {CONF_NUMBER: value}


def shorthand_input_pin(value):
    value = input_pin(value)
    return {CONF_NUMBER: value}


PCF8574_OUTPUT_PIN_SCHEMA = vol.Schema({
    vol.Required(CONF_PCF8574): cv.variable_id,
    vol.Required(CONF_NUMBER): vol.Coerce(int),
    vol.Optional(CONF_MODE): vol.All(vol.Upper, "OUTPUT"),
    vol.Optional(CONF_INVERTED, default=False): cv.boolean,
})

PCF8574_INPUT_PIN_SCHEMA = PCF8574_OUTPUT_PIN_SCHEMA.extend({
    vol.Optional(CONF_MODE): vol.All(vol.Upper, vol.Any("INPUT", "INPUT_PULLUP")),
})

GPIO_INTERNAL_OUTPUT_PIN_SCHEMA = vol.Any(shorthand_output_pin, GPIO_FULL_OUTPUT_PIN_SCHEMA)

GPIO_OUTPUT_PIN_SCHEMA = vol.Any(PCF8574_OUTPUT_PIN_SCHEMA, GPIO_INTERNAL_OUTPUT_PIN_SCHEMA)

GPIO_INTERNAL_INPUT_PIN_SCHEMA = vol.Any(shorthand_input_pin, GPIO_FULL_INPUT_PIN_SCHEMA)

GPIO_INPUT_PIN_SCHEMA = vol.Any(PCF8574_INPUT_PIN_SCHEMA, GPIO_INTERNAL_INPUT_PIN_SCHEMA)
