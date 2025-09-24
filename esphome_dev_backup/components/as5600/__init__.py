from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIR_PIN,
    CONF_DIRECTION,
    CONF_HYSTERESIS,
    CONF_ID,
    CONF_RANGE,
)

CODEOWNERS = ["@ammmze"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

as5600_ns = cg.esphome_ns.namespace("as5600")
AS5600Component = as5600_ns.class_("AS5600Component", cg.Component, i2c.I2CDevice)

DIRECTION = {
    "CLOCKWISE": 0,
    "COUNTERCLOCKWISE": 1,
}

POWER_MODE = {
    "NOMINAL": 0,
    "LOW1": 1,
    "LOW2": 2,
    "LOW3": 3,
}

HYSTERESIS = {
    "NONE": 0,
    "LSB1": 1,
    "LSB2": 2,
    "LSB3": 3,
}

SLOW_FILTER = {
    "16X": 0,
    "8X": 1,
    "4X": 2,
    "2X": 3,
}

FAST_FILTER = {
    "NONE": 0,
    "LSB6": 1,
    "LSB7": 2,
    "LSB9": 3,
    "LSB18": 4,
    "LSB21": 5,
    "LSB24": 6,
    "LSB10": 7,
}

CONF_RAW_ANGLE = "raw_angle"
CONF_RAW_POSITION = "raw_position"
CONF_WATCHDOG = "watchdog"
CONF_POWER_MODE = "power_mode"
CONF_SLOW_FILTER = "slow_filter"
CONF_FAST_FILTER = "fast_filter"
CONF_START_POSITION = "start_position"
CONF_END_POSITION = "end_position"


RESOLUTION = 4096
MAX_POSITION = RESOLUTION - 1
ANGLE_TO_POSITION = RESOLUTION / 360
POSITION_TO_ANGLE = 360 / RESOLUTION
# validate min range of 18deg (per datasheet) ... though i seem to get valid values down to a range of 192steps (16.875deg)
MIN_RANGE = round(18 * ANGLE_TO_POSITION)


def angle(min=-360, max=360):
    return cv.All(
        cv.float_with_unit("angle", "(°|deg)"), cv.float_range(min=min, max=max)
    )


def angle_to_position(value, min=-360, max=360):
    try:
        value = angle(min=min, max=max)(value)
        return (RESOLUTION + round(value * ANGLE_TO_POSITION)) % RESOLUTION
    except cv.Invalid as e:
        raise cv.Invalid(f"When using angle, {e.error_message}")


def percent_to_position(value):
    value = cv.possibly_negative_percentage(value)
    return (RESOLUTION + round(value * RESOLUTION)) % RESOLUTION


def position(min=-MAX_POSITION, max=MAX_POSITION):
    """Validate that the config option is a position.
    Accepts integers, degrees, or percentage (of 360 degrees).
    """

    def validator(value):
        if isinstance(value, str) and value.endswith("%"):
            value = percent_to_position(value)

        if isinstance(value, str) and (value.endswith("°") or value.endswith("deg")):
            return angle_to_position(
                value,
                min=round(min * POSITION_TO_ANGLE),
                max=round(max * POSITION_TO_ANGLE),
            )

        return cv.int_range(min=min, max=max)(value)

    return validator


def position_range():
    """Validate that value given is a valid range for the device.
    A valid range is one of the following:
    - a value of 0 (meaning full range)
    - 18 thru 360 degrees
    - negative 360 thru negative 18 degrees (notes: these are normalized to their positive values, accepting negatives is for convenience)
    """
    zero_validator = position(min=0, max=0)
    negative_validator = cv.Any(
        position(min=-MAX_POSITION, max=-MIN_RANGE),
        zero_validator,
    )
    positive_validator = cv.Any(
        position(min=MIN_RANGE, max=MAX_POSITION),
        zero_validator,
    )

    def validator(value):
        is_negative_str = isinstance(value, str) and value.startswith("-")
        is_negative_num = isinstance(value, (float, int)) and value < 0
        if is_negative_str or is_negative_num:
            return negative_validator(value)
        return positive_validator(value)

    return validator


def has_valid_range_config():
    """Validate that that the config start + end position results in a valid
    positional range, which must be >= 18degrees
    """
    range_validator = position_range()

    def validator(config):
        # if we don't have an end position, then there is nothing to do
        if CONF_END_POSITION not in config:
            return config

        # determine the range by taking the difference from the end and start
        range = config[CONF_END_POSITION] - config[CONF_START_POSITION]

        # but need to account for start position being greater than end position
        # where the range rolls back around the 0 position
        if config[CONF_END_POSITION] < config[CONF_START_POSITION]:
            range = RESOLUTION + config[CONF_END_POSITION] - config[CONF_START_POSITION]

        try:
            range_validator(range)
            return config
        except cv.Invalid as e:
            raise cv.Invalid(
                f"The range between start and end position is invalid. It was was {range} but {e.error_message}"
            )

    return validator


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS5600Component),
            cv.Optional(CONF_DIR_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_DIRECTION, default="CLOCKWISE"): cv.enum(
                DIRECTION, upper=True
            ),
            cv.Optional(CONF_WATCHDOG, default=False): cv.boolean,
            cv.Optional(CONF_POWER_MODE, default="NOMINAL"): cv.enum(
                POWER_MODE, upper=True, space=""
            ),
            cv.Optional(CONF_HYSTERESIS, default="NONE"): cv.enum(
                HYSTERESIS, upper=True, space=""
            ),
            cv.Optional(CONF_SLOW_FILTER, default="16X"): cv.enum(
                SLOW_FILTER, upper=True, space=""
            ),
            cv.Optional(CONF_FAST_FILTER, default="NONE"): cv.enum(
                FAST_FILTER, upper=True, space=""
            ),
            cv.Optional(CONF_START_POSITION, default=0): position(),
            cv.Optional(CONF_END_POSITION): position(),
            cv.Optional(CONF_RANGE): position_range(),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x36)),
    # ensure end_position and range are mutually exclusive
    cv.has_at_most_one_key(CONF_END_POSITION, CONF_RANGE),
    has_valid_range_config(),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_direction(config[CONF_DIRECTION]))
    cg.add(var.set_watchdog(config[CONF_WATCHDOG]))
    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
    cg.add(var.set_hysteresis(config[CONF_HYSTERESIS]))
    cg.add(var.set_slow_filter(config[CONF_SLOW_FILTER]))
    cg.add(var.set_fast_filter(config[CONF_FAST_FILTER]))
    cg.add(var.set_start_position(config[CONF_START_POSITION]))

    if dir_pin_config := config.get(CONF_DIR_PIN):
        pin = await cg.gpio_pin_expression(dir_pin_config)
        cg.add(var.set_dir_pin(pin))

    if (end_position_config := config.get(CONF_END_POSITION, None)) is not None:
        cg.add(var.set_end_position(end_position_config))

    if (range_config := config.get(CONF_RANGE, None)) is not None:
        cg.add(var.set_range(range_config))
