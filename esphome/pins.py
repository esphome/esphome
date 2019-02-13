from __future__ import division

import logging

import voluptuous as vol

import esphome.config_validation as cv
from esphome.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_PCF8574
from esphome.core import CORE
from esphome.cpp_types import Component, esphome_ns, io_ns

_LOGGER = logging.getLogger(__name__)

ESP8266_BASE_PINS = {
    'A0': 17, 'SS': 15, 'MOSI': 13, 'MISO': 12, 'SCK': 14, 'SDA': 4, 'SCL': 5, 'RX': 3, 'TX': 1
}

ESP8266_BOARD_PINS = {
    'd1': {'D0': 3, 'D1': 1, 'D2': 16, 'D3': 5, 'D4': 4, 'D5': 14, 'D6': 12, 'D7': 13, 'D8': 0,
           'D9': 2, 'D10': 15, 'D11': 13, 'D12': 14, 'D13': 14, 'D14': 4, 'D15': 5, 'LED': 2},
    'd1_mini': {'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13,
                'D8': 15, 'LED': 2},
    'd1_mini_lite': 'd1_mini',
    'd1_mini_pro': 'd1_mini',
    'esp01': {},
    'esp01_1m': {},
    'esp07': {},
    'esp12e': {},
    'esp210': {},
    'esp8285': {},
    'esp_wroom_02': {},
    'espduino': {'LED': 16},
    'espectro': {'LED': 15, 'BUTTON': 2},
    'espino': {'LED': 2, 'LED_RED': 2, 'LED_GREEN': 4, 'LED_BLUE': 5, 'BUTTON': 0},
    'espinotee': {'LED': 16},
    'espresso_lite_v1': {'LED': 16},
    'espresso_lite_v2': {'LED': 2},
    'gen4iod': {},
    'heltec_wifi_kit_8': 'd1_mini',
    'huzzah': {'LED': 0},
    'modwifi': {},
    'nodemcu': {'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13,
                'D8': 15, 'D9': 3, 'D10': 1, 'LED': 16},
    'nodemcuv2': 'nodemcu',
    'oak': {'P0': 2, 'P1': 5, 'P2': 0, 'P3': 3, 'P4': 1, 'P5': 4, 'P6': 15, 'P7': 13, 'P8': 12,
            'P9': 14, 'P10': 16, 'P11': 17, 'LED': 5},
    'phoenix_v1': {'LED': 16},
    'phoenix_v2': {'LED': 2},
    'sparkfunBlynk': 'thing',
    'thing': {'LED': 5, 'SDA': 2, 'SCL': 14},
    'thingdev': 'thing',
    'wifi_slot': {'LED': 2},
    'wifiduino': {'D0': 3, 'D1': 1, 'D2': 2, 'D3': 0, 'D4': 4, 'D5': 5, 'D6': 16, 'D7': 14,
                  'D8': 12, 'D9': 13, 'D10': 15, 'D11': 13, 'D12': 12, 'D13': 14},
    'wifinfo': {'LED': 12, 'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12,
                'D7': 13, 'D8': 15, 'D9': 3, 'D10': 1},
    'wio_link': {'LED': 2, 'GROVE': 15},
    'wio_node': 'nodemcu',
    'xinabox_cw01': {'SDA': 2, 'SCL': 14, 'LED': 5, 'LED_RED': 12, 'LED_GREEN': 13}
}

FLASH_SIZE_1_MB = 2**20
FLASH_SIZE_512_KB = FLASH_SIZE_1_MB // 2
FLASH_SIZE_2_MB = 2 * FLASH_SIZE_1_MB
FLASH_SIZE_4_MB = 4 * FLASH_SIZE_1_MB
FLASH_SIZE_16_MB = 16 * FLASH_SIZE_1_MB

ESP8266_FLASH_SIZES = {
    'd1': FLASH_SIZE_4_MB,
    'd1_mini': FLASH_SIZE_4_MB,
    'd1_mini_lite': FLASH_SIZE_1_MB,
    'd1_mini_pro': FLASH_SIZE_16_MB,
    'esp01': FLASH_SIZE_512_KB,
    'esp01_1m': FLASH_SIZE_1_MB,
    'esp07': FLASH_SIZE_4_MB,
    'esp12e': FLASH_SIZE_4_MB,
    'esp210': FLASH_SIZE_4_MB,
    'esp8285': FLASH_SIZE_1_MB,
    'esp_wroom_02': FLASH_SIZE_2_MB,
    'espduino': FLASH_SIZE_4_MB,
    'espectro': FLASH_SIZE_4_MB,
    'espino': FLASH_SIZE_4_MB,
    'espinotee': FLASH_SIZE_4_MB,
    'espresso_lite_v1': FLASH_SIZE_4_MB,
    'espresso_lite_v2': FLASH_SIZE_4_MB,
    'gen4iod': FLASH_SIZE_512_KB,
    'heltec_wifi_kit_8': FLASH_SIZE_4_MB,
    'huzzah': FLASH_SIZE_4_MB,
    'modwifi': FLASH_SIZE_2_MB,
    'nodemcu': FLASH_SIZE_4_MB,
    'nodemcuv2': FLASH_SIZE_4_MB,
    'oak': FLASH_SIZE_4_MB,
    'phoenix_v1': FLASH_SIZE_4_MB,
    'phoenix_v2': FLASH_SIZE_4_MB,
    'sparkfunBlynk': FLASH_SIZE_4_MB,
    'thing': FLASH_SIZE_512_KB,
    'thingdev': FLASH_SIZE_512_KB,
    'wifi_slot': FLASH_SIZE_1_MB,
    'wifiduino': FLASH_SIZE_4_MB,
    'wifinfo': FLASH_SIZE_1_MB,
    'wio_link': FLASH_SIZE_4_MB,
    'wio_node': FLASH_SIZE_4_MB,
    'xinabox_cw01': FLASH_SIZE_4_MB,
}

ESP8266_LD_SCRIPTS = {
    FLASH_SIZE_512_KB: ('eagle.flash.512k0.ld', 'eagle.flash.512k.ld'),
    FLASH_SIZE_1_MB: ('eagle.flash.1m0.ld', 'eagle.flash.1m.ld'),
    FLASH_SIZE_2_MB: ('eagle.flash.2m.ld', 'eagle.flash.2m.ld'),
    FLASH_SIZE_4_MB: ('eagle.flash.4m.ld', 'eagle.flash.4m.ld'),
    FLASH_SIZE_16_MB: ('eagle.flash.16m.ld', 'eagle.flash.16m14m.ld'),
}

ESP32_BASE_PINS = {
    'TX': 1, 'RX': 3, 'SDA': 21, 'SCL': 22, 'SS': 5, 'MOSI': 23, 'MISO': 19, 'SCK': 18, 'A0': 36,
    'A3': 39, 'A4': 32, 'A5': 33, 'A6': 34, 'A7': 35, 'A10': 4, 'A11': 0, 'A12': 2, 'A13': 15,
    'A14': 13, 'A15': 12, 'A16': 14, 'A17': 27, 'A18': 25, 'A19': 26, 'T0': 4, 'T1': 0, 'T2': 2,
    'T3': 15, 'T4': 13, 'T5': 12, 'T6': 14, 'T7': 27, 'T8': 33, 'T9': 32, 'DAC1': 25, 'DAC2': 26,
    'SVP': 36, 'SVN': 39,
}

ESP32_BOARD_PINS = {
    'alksesp32': {'D0': 40, 'D1': 41, 'D2': 15, 'D3': 2, 'D4': 0, 'D5': 4, 'D6': 16, 'D7': 17,
                  'D8': 5, 'D9': 18, 'D10': 19, 'D11': 21, 'D12': 22, 'D13': 23, 'A0': 32, 'A1': 33,
                  'A2': 25, 'A3': 26, 'A4': 27, 'A5': 14, 'A6': 12, 'A7': 15, 'L_R': 22, 'L_G': 17,
                  'L_Y': 23, 'L_B': 5, 'L_RGB_R': 4, 'L_RGB_G': 21, 'L_RGB_B': 16, 'SW1': 15,
                  'SW2': 2, 'SW3': 0, 'POT1': 32, 'POT2': 33, 'PIEZO1': 19, 'PIEZO2': 18,
                  'PHOTO': 25, 'DHT_PIN': 26, 'S1': 4, 'S2': 16, 'S3': 18, 'S4': 19, 'S5': 21,
                  'SDA': 27, 'SCL': 14, 'SS': 19, 'MOSI': 21, 'MISO': 22, 'SCK': 23},
    'esp-wrover-kit': {},
    'esp32-evb': {'BUTTON': 34, 'SDA': 13, 'SCL': 16, 'SS': 17, 'MOSI': 2, 'MISO': 15, 'SCK': 14},
    'esp32-gateway': {'LED': 33, 'BUTTON': 34, 'SCL': 16, 'SDA': 17},
    'esp320': {'LED': 5, 'SDA': 2, 'SCL': 14, 'SS': 15, 'MOSI': 13, 'MISO': 12, 'SCK': 14},
    'esp32dev': {},
    'esp32doit-devkit-v1': {'LED': 2},
    'esp32thing': {'LED': 5, 'BUTTON': 0, 'SS': 2},
    'esp32vn-iot-uno': {},
    'espea32': {'LED': 5, 'BUTTON': 0},
    'espectro32': {'LED': 15, 'SD_SS': 33},
    'espino32': {'LED': 16, 'BUTTON': 0},
    'featheresp32': {'LED': 13, 'TX': 17, 'RX': 16, 'SDA': 23, 'SS': 2, 'MOSI': 18, 'SCK': 5,
                     'A0': 26, 'A1': 25, 'A2': 34, 'A4': 36, 'A5': 4, 'A6': 14, 'A7': 32, 'A8': 15,
                     'A9': 33, 'A10': 27, 'A11': 12, 'A12': 13, 'A13': 35},
    'firebeetle32': {'LED': 2},
    'heltec_wifi_kit_32': {'LED': 25, 'BUTTON': 0, 'A1': 37, 'A2': 38},
    'heltec_wifi_lora_32': {'LED': 25, 'BUTTON': 0, 'SDA': 4, 'SCL': 15, 'SS': 18, 'MOSI': 27,
                            'SCK': 5, 'A1': 37, 'A2': 38, 'T8': 32, 'T9': 33, 'DAC1': 26,
                            'DAC2': 25, 'OLED_SCL': 15, 'OLED_SDA': 4, 'OLED_RST': 16,
                            'LORA_SCK': 5, 'LORA_MOSI': 27, 'LORA_MISO': 19, 'LORA_CS': 18,
                            'LORA_RST': 14, 'LORA_IRQ': 26},
    'hornbill32dev': {'LED': 13, 'BUTTON': 0},
    'hornbill32minima': {'SS': 2},
    'intorobot': {'LED': 4, 'LED_RED': 27, 'LED_GREEN': 21, 'LED_BLUE': 22,
                  'BUTTON': 0, 'SDA': 23, 'SCL': 19, 'MOSI': 16, 'MISO': 17, 'A1': 39, 'A2': 35,
                  'A3': 25, 'A4': 26, 'A5': 14, 'A6': 12, 'A7': 15, 'A8': 13, 'A9': 2, 'D0': 19,
                  'D1': 23, 'D2': 18, 'D3': 17, 'D4': 16, 'D5': 5, 'D6': 4, 'T0': 19, 'T1': 23,
                  'T2': 18, 'T3': 17, 'T4': 16, 'T5': 5, 'T6': 4},
    'lolin32': {'LED': 5},
    'lolin_d32': {'LED': 5, 'VBAT': 35},
    'lolin_d32_pro': {'LED': 5, 'VBAT': 35, 'TF_CS': 4, 'TS_CS': 12, 'TFT_CS': 14, 'TFT_LED': 32,
                      'TFT_RST': 33, 'TFT_DC': 27},
    'm5stack-core-esp32': {'TXD2': 17, 'RXD2': 16, 'G23': 23, 'G19': 19, 'G18': 18, 'G3': 3,
                           'G16': 16, 'G21': 21, 'G2': 2, 'G12': 12, 'G15': 15, 'G35': 35,
                           'G36': 36, 'G25': 25, 'G26': 26, 'G1': 1, 'G17': 17, 'G22': 22, 'G5': 5,
                           'G13': 13, 'G0': 0, 'G34': 34, 'ADC1': 35, 'ADC2': 36},
    'm5stack-fire': {'G23': 23, 'G19': 19, 'G18': 18, 'G3': 3, 'G16': 16, 'G21': 21, 'G2': 2,
                     'G12': 12, 'G15': 15, 'G35': 35, 'G36': 36, 'G25': 25, 'G26': 26, 'G1': 1,
                     'G17': 17, 'G22': 22, 'G5': 5, 'G13': 13, 'G0': 0, 'G34': 34, 'ADC1': 35,
                     'ADC2': 36},
    'mhetesp32devkit': {'LED': 2},
    'mhetesp32minikit': {'LED': 2},
    'microduino-core-esp32': {'SDA': 22, 'SCL': 21, 'SDA1': 12, 'SCL1': 13, 'A0': 12, 'A1': 13,
                              'A2': 15, 'A3': 4, 'A6': 38, 'A7': 37, 'A8': 32, 'A9': 33, 'A10': 25,
                              'A11': 26, 'A12': 27, 'A13': 14, 'D0': 3, 'D1': 1, 'D2': 16, 'D3': 17,
                              'D4': 32, 'D5': 33, 'D6': 25, 'D7': 26, 'D8': 27, 'D9': 14, 'D10': 5,
                              'D11': 23, 'D12': 19, 'D13': 18, 'D14': 12, 'D15': 13, 'D16': 15,
                              'D17': 4, 'D18': 22, 'D19': 21, 'D20': 38, 'D21': 37},
    'nano32': {'LED': 16, 'BUTTON': 0},
    'nina_w10': {'LED_GREEN': 33, 'LED_RED': 23, 'LED_BLUE': 21, 'SW1': 33, 'SW2': 27, 'SDA': 12,
                 'SCL': 13, 'D0': 3, 'D1': 1, 'D2': 26, 'D3': 25, 'D4': 35, 'D5': 27, 'D6': 22,
                 'D7': 0, 'D8': 15, 'D9': 14, 'D10': 5, 'D11': 19, 'D12': 23, 'D13': 18, 'D14': 13,
                 'D15': 12, 'D16': 32, 'D17': 33, 'D18': 21, 'D19': 34, 'D20': 36, 'D21': 39},
    'node32s': {},
    'nodemcu-32s': {'LED': 2, 'BUTTON': 0},
    'odroid_esp32': {'LED': 2, 'SDA': 15, 'SCL': 4, 'SS': 22, 'ADC1': 35, 'ADC2': 36},
    'onehorse32dev': {'LED': 5, 'BUTTON': 0, 'A1': 37, 'A2': 38},
    'pico32': {},
    'pocket_32': {'LED': 16},
    'quantum': {},
    'ttgo-lora32-v1': {'LED': 2, 'BUTTON': 0, 'SS': 18, 'MOSI': 27, 'SCK': 5, 'A1': 37, 'A2': 38,
                       'T8': 32, 'T9': 33, 'DAC1': 26, 'DAC2': 25, 'OLED_SDA': 4, 'OLED_SCL': 15,
                       'OLED_RST': 16, 'LORA_SCK': 5, 'LORA_MISO': 19, 'LORA_MOSI': 27,
                       'LORA_CS': 18, 'LORA_RST': 14, 'LORA_IRQ': 26},
    'wemosbat': 'pocket_32',
    'widora-air': {'LED': 25, 'BUTTON': 0, 'SDA': 23, 'SCL': 19, 'MOSI': 16, 'MISO': 17, 'A1': 39,
                   'A2': 35, 'A3': 25, 'A4': 26, 'A5': 14, 'A6': 12, 'A7': 15, 'A8': 13, 'A9': 2,
                   'D0': 19, 'D1': 23, 'D2': 18, 'D3': 17, 'D4': 16, 'D5': 5, 'D6': 4, 'T0': 19,
                   'T1': 23, 'T2': 18, 'T3': 17, 'T4': 16, 'T5': 5, 'T6': 4},
    'xinabox_cw02': {'LED': 27},
}


def _lookup_pin(value):
    if CORE.is_esp8266:
        board_pins = ESP8266_BOARD_PINS.get(CORE.board, {})
        base_pins = ESP8266_BASE_PINS
    elif CORE.is_esp32:
        board_pins = ESP32_BOARD_PINS.get(CORE.board, {})
        base_pins = ESP32_BASE_PINS
    else:
        raise NotImplementedError

    while isinstance(board_pins, str):
        board_pins = ESP8266_BOARD_PINS.get(board_pins, {})
    if value in board_pins:
        return board_pins[value]
    if value in base_pins:
        return base_pins[value]
    raise vol.Invalid(u"Cannot resolve pin name '{}' for board {}.".format(value, CORE.board))


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
    return _lookup_pin(value)


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if CORE.is_esp32:
        if value < 0 or value > 39:
            raise vol.Invalid(u"ESP32: Invalid pin number: {}".format(value))
        if 6 <= value <= 11:
            _LOGGER.warning(u"ESP32: Pin %s (6-11) might already be used by the "
                            u"flash interface. Be warned.", value)
        if value in (20, 24, 28, 29, 30, 31):
            _LOGGER.warning(u"ESP32: Pin %s (20, 24, 28-31) can usually not be used. "
                            u"Be warned.", value)
        return value
    if CORE.is_esp8266:
        if 6 <= value <= 11:
            _LOGGER.warning(u"ESP8266: Pin %s (6-11) might already be used by the "
                            u"flash interface. Be warned.", value)
        if value < 0 or value > 17:
            raise vol.Invalid(u"ESP8266: Invalid pin number: {}".format(value))
        return value
    raise NotImplementedError


def input_pin(value):
    return validate_gpio_pin(value)


def input_pullup_pin(value):
    value = input_pin(value)
    if CORE.is_esp32:
        return output_pin(value)
    if CORE.is_esp8266:
        if value == 0:
            raise vol.Invalid("GPIO Pin 0 does not support pullup pin mode. "
                              "Please choose another pin.")
        return value
    raise NotImplementedError


def output_pin(value):
    value = validate_gpio_pin(value)
    if CORE.is_esp32:
        if 34 <= value <= 39:
            raise vol.Invalid(u"ESP32: GPIO{} (34-39) can only be used as an "
                              u"input pin.".format(value))
        return value
    if CORE.is_esp8266:
        return value
    raise NotImplementedError


def analog_pin(value):
    value = validate_gpio_pin(value)
    if CORE.is_esp32:
        if 32 <= value <= 39:  # ADC1
            return value
        raise vol.Invalid(u"ESP32: Only pins 32 though 39 support ADC.")
    elif CORE.is_esp8266:
        if value == 17:  # A0
            return value
        raise vol.Invalid(u"ESP8266: Only pin A0 (17) supports ADC.")
    raise NotImplementedError


input_output_pin = vol.All(input_pin, output_pin)

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
    if CORE.is_esp32:
        return cv.one_of(*PIN_MODES_ESP32, upper=True)(value)
    if CORE.is_esp8266:
        return cv.one_of(*PIN_MODES_ESP8266, upper=True)(value)
    raise NotImplementedError


GPIO_FULL_OUTPUT_PIN_SCHEMA = vol.Schema({
    vol.Required(CONF_NUMBER): output_pin,
    vol.Optional(CONF_MODE): pin_mode,
    vol.Optional(CONF_INVERTED): cv.boolean,
})

GPIO_FULL_INPUT_PIN_SCHEMA = vol.Schema({
    vol.Required(CONF_NUMBER): input_pin,
    vol.Optional(CONF_MODE): pin_mode,
    vol.Optional(CONF_INVERTED): cv.boolean,
})


def shorthand_output_pin(value):
    value = output_pin(value)
    return {CONF_NUMBER: value}


def shorthand_input_pin(value):
    value = input_pin(value)
    return {CONF_NUMBER: value}


def shorthand_input_pullup_pin(value):
    value = input_pullup_pin(value)
    return {CONF_NUMBER: value}


I2CDevice = esphome_ns.class_('I2CDevice')
PCF8574Component = io_ns.class_('PCF8574Component', Component, I2CDevice)

PCF8574_OUTPUT_PIN_SCHEMA = vol.Schema({
    vol.Required(CONF_PCF8574): cv.use_variable_id(PCF8574Component),
    vol.Required(CONF_NUMBER): vol.Coerce(int),
    vol.Optional(CONF_MODE): cv.one_of("OUTPUT", upper=True),
    vol.Optional(CONF_INVERTED, default=False): cv.boolean,
})

PCF8574_INPUT_PIN_SCHEMA = PCF8574_OUTPUT_PIN_SCHEMA.extend({
    vol.Optional(CONF_MODE): cv.one_of("INPUT", "INPUT_PULLUP", upper=True),
})


def internal_gpio_output_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_OUTPUT_PIN_SCHEMA(value)
    return shorthand_output_pin(value)


def gpio_output_pin_schema(value):
    if isinstance(value, dict) and CONF_PCF8574 in value:
        return PCF8574_OUTPUT_PIN_SCHEMA(value)
    return internal_gpio_output_pin_schema(value)


def internal_gpio_input_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_INPUT_PIN_SCHEMA(value)
    return shorthand_input_pin(value)


def gpio_input_pin_schema(value):
    if isinstance(value, dict) and CONF_PCF8574 in value:
        return PCF8574_INPUT_PIN_SCHEMA(value)
    return internal_gpio_input_pin_schema(value)


def internal_gpio_input_pullup_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_INPUT_PIN_SCHEMA(value)
    return shorthand_input_pullup_pin(value)


def gpio_input_pullup_pin_schema(value):
    if isinstance(value, dict) and CONF_PCF8574 in value:
        return PCF8574_INPUT_PIN_SCHEMA(value)
    return internal_gpio_input_pin_schema(value)
