import logging

import esphome.config_validation as cv
from esphome.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER
from esphome.core import CORE
from esphome.util import SimpleRegistry
from esphome import boards

_LOGGER = logging.getLogger(__name__)


def _lookup_pin(value):
    if CORE.is_esp8266:
        board_pins_dict = boards.ESP8266_BOARD_PINS
        base_pins = boards.ESP8266_BASE_PINS
    elif CORE.is_esp32:
        if CORE.board in boards.ESP32_C3_BOARD_PINS:
            board_pins_dict = boards.ESP32_C3_BOARD_PINS
            base_pins = boards.ESP32_C3_BASE_PINS
        else:
            board_pins_dict = boards.ESP32_BOARD_PINS
            base_pins = boards.ESP32_BASE_PINS
    else:
        raise NotImplementedError

    board_pins = board_pins_dict.get(CORE.board, {})

    # Resolved aliased board pins (shorthand when two boards have the same pin configuration)
    while isinstance(board_pins, str):
        board_pins = board_pins_dict[board_pins]

    if value in board_pins:
        return board_pins[value]
    if value in base_pins:
        return base_pins[value]
    raise cv.Invalid(f"Cannot resolve pin name '{value}' for board {CORE.board}.")


def _translate_pin(value):
    if isinstance(value, dict) or value is None:
        raise cv.Invalid(
            "This variable only supports pin numbers, not full pin schemas "
            "(with inverted and mode)."
        )
    if isinstance(value, int):
        return value
    try:
        return int(value)
    except ValueError:
        pass
    if value.startswith("GPIO"):
        return cv.Coerce(int)(value[len("GPIO") :].strip())
    return _lookup_pin(value)


_ESP_SDIO_PINS = {
    6: "Flash Clock",
    7: "Flash Data 0",
    8: "Flash Data 1",
    11: "Flash Command",
}

_ESP32C3_SDIO_PINS = {
    12: "Flash IO3/HOLD#",
    13: "Flash IO2/WP#",
    14: "Flash CS#",
    15: "Flash CLK",
    16: "Flash IO0/DI",
    17: "Flash IO1/DO",
}


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if CORE.is_esp32_c3:
        if value < 0 or value > 22:
            raise cv.Invalid(f"ESP32-C3: Invalid pin number: {value}")
        if value in _ESP32C3_SDIO_PINS:
            raise cv.Invalid(
                "This pin cannot be used on ESP32-C3s and is already used by "
                "the flash interface (function: {})".format(_ESP_SDIO_PINS[value])
            )
        return value
    if CORE.is_esp32:
        if value < 0 or value > 39:
            raise cv.Invalid(f"ESP32: Invalid pin number: {value}")
        if value in _ESP_SDIO_PINS:
            raise cv.Invalid(
                "This pin cannot be used on ESP32s and is already used by "
                "the flash interface (function: {})".format(_ESP_SDIO_PINS[value])
            )
        if 9 <= value <= 10:
            _LOGGER.warning(
                "ESP32: Pin %s (9-10) might already be used by the "
                "flash interface in QUAD IO flash mode.",
                value,
            )
        if value in (20, 24, 28, 29, 30, 31):
            # These pins are not exposed in GPIO mux (reason unknown)
            # but they're missing from IO_MUX list in datasheet
            raise cv.Invalid(f"The pin GPIO{value} is not usable on ESP32s.")
        return value
    if CORE.is_esp8266:
        if value < 0 or value > 17:
            raise cv.Invalid(f"ESP8266: Invalid pin number: {value}")
        if value in _ESP_SDIO_PINS:
            raise cv.Invalid(
                "This pin cannot be used on ESP8266s and is already used by "
                "the flash interface (function: {})".format(_ESP_SDIO_PINS[value])
            )
        if 9 <= value <= 10:
            _LOGGER.warning(
                "ESP8266: Pin %s (9-10) might already be used by the "
                "flash interface in QUAD IO flash mode.",
                value,
            )
        return value
    raise NotImplementedError


def input_pin(value):
    value = validate_gpio_pin(value)
    if CORE.is_esp8266 and value == 17:
        raise cv.Invalid("GPIO17 (TOUT) is an analog-only pin on the ESP8266.")
    return value


def input_pullup_pin(value):
    value = input_pin(value)
    if CORE.is_esp32:
        return output_pin(value)
    if CORE.is_esp8266:
        if value == 0:
            raise cv.Invalid(
                "GPIO Pin 0 does not support pullup pin mode. "
                "Please choose another pin."
            )
        return value
    raise NotImplementedError


def output_pin(value):
    value = validate_gpio_pin(value)
    if CORE.is_esp32:
        if 34 <= value <= 39:
            raise cv.Invalid(
                "ESP32: GPIO{} (34-39) can only be used as an "
                "input pin.".format(value)
            )
        return value
    if CORE.is_esp8266:
        if value == 17:
            raise cv.Invalid("GPIO17 (TOUT) is an analog-only pin on the ESP8266.")
        return value
    raise NotImplementedError


def analog_pin(value):
    value = validate_gpio_pin(value)
    if CORE.is_esp32:
        if CORE.is_esp32_c3:
            if 0 <= value <= 4:  # ADC1
                return value
            raise cv.Invalid("ESP32-C3: Only pins 0 though 4 support ADC.")
        if 32 <= value <= 39:  # ADC1
            return value
        raise cv.Invalid("ESP32: Only pins 32 though 39 support ADC.")
    if CORE.is_esp8266:
        if value == 17:  # A0
            return value
        raise cv.Invalid("ESP8266: Only pin A0 (GPIO17) supports ADC.")
    raise NotImplementedError


input_output_pin = cv.All(input_pin, output_pin)

PIN_MODES_ESP8266 = [
    "INPUT",
    "OUTPUT",
    "INPUT_PULLUP",
    "OUTPUT_OPEN_DRAIN",
    "SPECIAL",
    "FUNCTION_1",
    "FUNCTION_2",
    "FUNCTION_3",
    "FUNCTION_4",
    "FUNCTION_0",
    "WAKEUP_PULLUP",
    "WAKEUP_PULLDOWN",
    "INPUT_PULLDOWN_16",
]
PIN_MODES_ESP32 = [
    "INPUT",
    "OUTPUT",
    "INPUT_PULLUP",
    "OUTPUT_OPEN_DRAIN",
    "SPECIAL",
    "FUNCTION_1",
    "FUNCTION_2",
    "FUNCTION_3",
    "FUNCTION_4",
    "PULLUP",
    "PULLDOWN",
    "INPUT_PULLDOWN",
    "OPEN_DRAIN",
    "FUNCTION_5",
    "FUNCTION_6",
    "ANALOG",
]


def pin_mode(value):
    if CORE.is_esp32:
        return cv.one_of(*PIN_MODES_ESP32, upper=True)(value)
    if CORE.is_esp8266:
        return cv.one_of(*PIN_MODES_ESP8266, upper=True)(value)
    raise NotImplementedError


GPIO_FULL_OUTPUT_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NUMBER): output_pin,
        cv.Optional(CONF_MODE, default="OUTPUT"): pin_mode,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)

GPIO_FULL_INPUT_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NUMBER): input_pin,
        cv.Optional(CONF_MODE, default="INPUT"): pin_mode,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)

GPIO_FULL_INPUT_PULLUP_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NUMBER): input_pin,
        cv.Optional(CONF_MODE, default="INPUT_PULLUP"): pin_mode,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)

GPIO_FULL_ANALOG_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NUMBER): analog_pin,
        cv.Optional(CONF_MODE, default="INPUT"): pin_mode,
    }
)


def shorthand_output_pin(value):
    value = output_pin(value)
    return GPIO_FULL_OUTPUT_PIN_SCHEMA({CONF_NUMBER: value})


def shorthand_input_pin(value):
    value = input_pin(value)
    return GPIO_FULL_INPUT_PIN_SCHEMA({CONF_NUMBER: value})


def shorthand_input_pullup_pin(value):
    value = input_pullup_pin(value)
    return GPIO_FULL_INPUT_PIN_SCHEMA(
        {
            CONF_NUMBER: value,
            CONF_MODE: "INPUT_PULLUP",
        }
    )


def shorthand_analog_pin(value):
    value = analog_pin(value)
    return GPIO_FULL_ANALOG_PIN_SCHEMA({CONF_NUMBER: value})


def validate_has_interrupt(value):
    if CORE.is_esp8266:
        if value[CONF_NUMBER] >= 16:
            raise cv.Invalid(
                "Pins GPIO16 and GPIO17 do not support interrupts and cannot be used "
                "here, got {}".format(value[CONF_NUMBER])
            )
    return value


PIN_SCHEMA_REGISTRY = SimpleRegistry()


def internal_gpio_output_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_OUTPUT_PIN_SCHEMA(value)
    return shorthand_output_pin(value)


def gpio_output_pin_schema(value):
    if isinstance(value, dict):
        for key, entry in PIN_SCHEMA_REGISTRY.items():
            if key in value:
                return entry[1][0](value)
    return internal_gpio_output_pin_schema(value)


def internal_gpio_input_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_INPUT_PIN_SCHEMA(value)
    return shorthand_input_pin(value)


def internal_gpio_analog_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_ANALOG_PIN_SCHEMA(value)
    return shorthand_analog_pin(value)


def gpio_input_pin_schema(value):
    if isinstance(value, dict):
        for key, entry in PIN_SCHEMA_REGISTRY.items():
            if key in value:
                return entry[1][1](value)
    return internal_gpio_input_pin_schema(value)


def internal_gpio_input_pullup_pin_schema(value):
    if isinstance(value, dict):
        return GPIO_FULL_INPUT_PULLUP_PIN_SCHEMA(value)
    return shorthand_input_pullup_pin(value)


def gpio_input_pullup_pin_schema(value):
    if isinstance(value, dict):
        for key, entry in PIN_SCHEMA_REGISTRY.items():
            if key in value:
                return entry[1][1](value)
    return internal_gpio_input_pullup_pin_schema(value)
