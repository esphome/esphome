import esphome.codegen as cg
from esphome.components import display, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_CONTINUOUS,
    CONF_DEVICE,
    CONF_ID,
    CONF_LAMBDA,
)

DEPENDENCIES = ["i2c"]

ht16k33_char_ns = cg.esphome_ns.namespace("ht16k33_char")

CONF_MAX_BUFFER_LENGTH = "max_buffer_length"
CONF_SCROLL = "scroll"
CONF_SCROLL_SPEED = "scroll_speed"
CONF_SCROLL_DWELL = "scroll_dwell"
CONF_SCROLL_DELAY = "scroll_delay"
CONF_SECONDARY_DISPLAYS = "secondary_displays"

CONF_ADD_CHARACTERS = "add_characters"
CONF_REMOVE_CHARACTERS = "remove_characters"

CONFIG_SECONDARY = cv.Schema({cv.GenerateID(): cv.declare_id(i2c.I2CDevice)}).extend(
    i2c.i2c_device_schema(None)
)

HT16k33Char_BaseClassType = ht16k33_char_ns.class_(
    "HT16k33CharComponent", cg.PollingComponent, i2c.I2CDevice
)


# Formatting functions. These functions convert the input character codes to the proper format for the various devices.
#   These functions are implemented so that the `add_characters` config option takes a universal format. The format for
#   the `add_characters` function matches the wiring of the Adafruit 7 and 14 segment devices. For the other devices
#   these functions are used to flip the bits around in the provided character code to match the wiring of that device.
def format_none(input_code):
    return input_code


def format_7seg_flip(input_code):
    return (
        ((input_code & 0x0007) << 3)
        | ((input_code & 0x0038) >> 3)
        | (input_code & 0x0040)
    )


def format_14seg_flip(input_code):
    return (
        ((input_code & 0x0007) << 3)
        | ((input_code & 0x0038) >> 3)
        | ((input_code & 0x0040) << 1)
        | ((input_code & 0x0080) >> 1)
        | ((input_code & 0x0100) << 5)
        | ((input_code & 0x0200) << 3)
        | ((input_code & 0x0400) << 1)
        | ((input_code & 0x0800) >> 1)
        | ((input_code & 0x1000) >> 3)
        | ((input_code & 0x2000) >> 5)
    )


def format_14seg_sparkfun(input_code):
    tempval = ((input_code & 0xFF80) << 1) | (input_code & 0x7F)
    if ((tempval & 0x1000) != 0x0000) and ((tempval & 0x4000) == 0x0000):
        # Segment L is lit, need to switch to segment N
        tempval = (tempval | 0x4000) & ~(0x1000)
    elif ((tempval & 0x4000) != 0x0000) and ((tempval & 0x1000) == 0x0000):
        # Segment N is lit, need to switch to segment L
        tempval = (tempval | 0x1000) & ~(0x4000)
    return tempval


def format_14seg_sparkfun_flip(input_code):
    tempval = format_14seg_sparkfun(input_code)
    return (
        ((tempval & 0x0007) << 3)
        | ((tempval & 0x0038) >> 3)
        | ((tempval & 0x0E00) << 3)
        | ((tempval & 0x7000) >> 3)
        | ((tempval & 0x0040) << 2)
        | ((tempval & 0x0100) >> 2)
    )


def validate_added_chars(value_to_validate):
    # Check if the value is a dictionary
    if not isinstance(value_to_validate, dict):
        raise cv.Invalid("add_characters expects a dictionary")

    for key, value in value_to_validate.items():
        # Validate the keys: they need to be a single character string.
        if not isinstance(key, str):
            raise cv.Invalid("dictionary keys must be strings")
        if len(key) > 1:
            # It appears that python correctly determines the length of the
            # string in characters instead of bytes. A string with a single
            # multi-byte character still returns 1 from the len() function.
            raise cv.Invalid("dictionary keys must be a single character")

        # Validate the values: they need to be integers from 0-0xFFFF
        if not isinstance(value, int):
            raise cv.Invalid("dictionary values must be integers")
        if (value < 0) or (value > 0xFFFF):
            raise cv.Invalid("dictionary values must be between 0 and 0xFFFF")

    return value_to_validate


def validate_removed_chars(value_to_validate):
    if not isinstance(value_to_validate, list):
        # If the entry is not a list, make it into a list.
        value_to_validate = [value_to_validate]
    for list_item in value_to_validate:
        if not isinstance(list_item, str):
            raise cv.Invalid("List must contain single character strings only")
        if len(list_item) > 1:
            # It appears that python correctly determines the length of the
            # string in characters instead of bytes. A string with a single
            # multi-byte character still returns 1 from the len() function.
            raise cv.Invalid("List must contain single character strings only")

    return value_to_validate


# A dictionary for supported device types:
#  -The key is what the user would put in the YAML file to select this device.
#  -The value is a dictionary that contains the keys:
#     `CLASS_NAME`: The name of the class that implements the device.
#     `FORMAT_FUNCTION`: A python function defined in this file that converts
#                        a digit code from the standard format to whatever
#                        format the device expects.
HT16K33_DEVICE_TYPES = {
    "ADAFRUIT_7_SEG_1.2IN": {
        "CLASS_NAME": "Adafruit7SegLarge",
        "FORMAT_FUNCTION": format_none,
    },
    "ADAFRUIT_7_SEG_1.2IN_FLIPPED": {
        "CLASS_NAME": "Adafruit7SegLargeFlip",
        "FORMAT_FUNCTION": format_7seg_flip,
    },
    "ADAFRUIT_7_SEG_.56IN": {
        "CLASS_NAME": "Adafruit7Seg",
        "FORMAT_FUNCTION": format_none,
    },
    "ADAFRUIT_7_SEG_.56IN_FLIPPED": {
        "CLASS_NAME": "Adafruit7SegFlip",
        "FORMAT_FUNCTION": format_7seg_flip,
    },
    "ADAFRUIT_14_SEG": {
        "CLASS_NAME": "Adafruit14Seg",
        "FORMAT_FUNCTION": format_none,
    },
    "ADAFRUIT_14_SEG_FLIPPED": {
        "CLASS_NAME": "Adafruit14SegFlip",
        "FORMAT_FUNCTION": format_14seg_flip,
    },
    "SPARKFUN_14_SEG": {
        "CLASS_NAME": "Sparkfun14Seg",
        "FORMAT_FUNCTION": format_14seg_sparkfun,
    },
    "SPARKFUN_14_SEG_FLIPPED": {
        "CLASS_NAME": "Sparkfun14SegFlip",
        "FORMAT_FUNCTION": format_14seg_sparkfun_flip,
    },
}

HT16k33Char_BaseClassTypeRef = HT16k33Char_BaseClassType.operator("ref")

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HT16k33Char_BaseClassType),
            cv.Required(CONF_DEVICE): cv.enum(HT16K33_DEVICE_TYPES, upper=True),
            cv.Optional(CONF_MAX_BUFFER_LENGTH, default=8): cv.int_range(
                min=4, max=255
            ),
            cv.Optional(CONF_BRIGHTNESS, default=15): cv.int_range(min=1, max=16),
            cv.Optional(CONF_SECONDARY_DISPLAYS): cv.ensure_list(CONFIG_SECONDARY),
            cv.Optional(CONF_CONTINUOUS, default=False): cv.boolean,
            cv.Optional(CONF_SCROLL, default=False): cv.boolean,
            cv.Optional(
                CONF_SCROLL_SPEED, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SCROLL_DWELL, default="2s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SCROLL_DELAY, default="5s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ADD_CHARACTERS): validate_added_chars,
            cv.Optional(CONF_REMOVE_CHARACTERS): validate_removed_chars,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x70))
)


async def to_code(config):
    ClassType = ht16k33_char_ns.class_(
        HT16K33_DEVICE_TYPES[config[CONF_DEVICE]]["CLASS_NAME"],
        HT16k33Char_BaseClassType,
    )
    ClassInstantiation = ClassType.new()
    var = cg.Pvariable(config[CONF_ID], ClassInstantiation, ClassType)

    await i2c.register_i2c_device(var, config)
    await display.register_display(var, config)
    cg.add(var.set_buffer_max_size(config[CONF_MAX_BUFFER_LENGTH]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(HT16k33Char_BaseClassTypeRef, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))

    if config[CONF_SCROLL]:
        cg.add(var.set_scroll(True))
        cg.add(var.set_continuous(config[CONF_CONTINUOUS]))
        cg.add(var.set_scroll_speed(config[CONF_SCROLL_SPEED]))
        cg.add(var.set_scroll_dwell(config[CONF_SCROLL_DWELL]))
        cg.add(var.set_scroll_delay(config[CONF_SCROLL_DELAY]))

    if CONF_SECONDARY_DISPLAYS in config:
        for conf in config[CONF_SECONDARY_DISPLAYS]:
            disp = cg.new_Pvariable(conf[CONF_ID])
            await i2c.register_i2c_device(disp, conf)
            cg.add(var.add_secondary_display(disp))

    if CONF_ADD_CHARACTERS in config:
        for char_to_add, value_to_add in config[CONF_ADD_CHARACTERS].items():
            cg.add(
                var.add_char(
                    char_to_add,
                    HT16K33_DEVICE_TYPES[config[CONF_DEVICE]]["FORMAT_FUNCTION"](
                        value_to_add
                    ),
                )
            )

    if CONF_REMOVE_CHARACTERS in config:
        for char_to_remove in config[CONF_REMOVE_CHARACTERS]:
            cg.add(var.remove_char(char_to_remove))
