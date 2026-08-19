import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_COL, CONF_KEY, CONF_ROW

from .. import CONF_KEYPAD_ID, TCA8418Component, tca8418_ns

DEPENDENCIES = ["tca8418"]

CONF_KEY_CODE = "key_code"

TCA8418BinarySensor = tca8418_ns.class_(
    "TCA8418BinarySensor", binary_sensor.BinarySensor
)


def _validate(config):
    has_position = CONF_ROW in config or CONF_COL in config
    selectors = [has_position, CONF_KEY in config, CONF_KEY_CODE in config]
    if sum(selectors) > 1:
        raise cv.Invalid(
            f"Use only one of a position ({CONF_ROW}/{CONF_COL}), "
            f"'{CONF_KEY}' or '{CONF_KEY_CODE}' to identify the key"
        )
    if has_position:
        if CONF_ROW not in config:
            raise cv.Invalid(f"Missing '{CONF_ROW}'")
        if CONF_COL not in config:
            raise cv.Invalid(f"Missing '{CONF_COL}'")
    elif (key := config.get(CONF_KEY)) is not None:
        if len(key) != 1:
            raise cv.Invalid("Key must be a single character", path=[CONF_KEY])
    elif not any(selectors):
        raise cv.Invalid(
            f"Identify the key with a position ({CONF_ROW}/{CONF_COL}), "
            f"'{CONF_KEY}' or '{CONF_KEY_CODE}'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(TCA8418BinarySensor).extend(
        {
            cv.GenerateID(CONF_KEYPAD_ID): cv.use_id(TCA8418Component),
            cv.Optional(CONF_ROW): cv.int_range(min=0, max=7),
            cv.Optional(CONF_COL): cv.int_range(min=0, max=9),
            cv.Optional(CONF_KEY): cv.string_strict,
            cv.Optional(CONF_KEY_CODE): cv.int_range(min=1, max=114),
        }
    ),
    _validate,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    if (row := config.get(CONF_ROW)) is not None:
        cg.add(var.set_position(row, config[CONF_COL]))
    elif (key := config.get(CONF_KEY)) is not None:
        cg.add(var.set_key_char(ord(key)))
    else:
        cg.add(var.set_key(config[CONF_KEY_CODE]))
    keypad = await cg.get_variable(config[CONF_KEYPAD_ID])
    cg.add(keypad.register_listener(var))
