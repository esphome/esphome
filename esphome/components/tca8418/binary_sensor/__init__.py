import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
import esphome.config_validation as cv
from esphome.const import CONF_COL, CONF_KEY, CONF_ROW
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_KEYPAD_ID, TCA8418Component, tca8418_ns

DEPENDENCIES = ["tca8418"]

CONF_KEY_CODE = "key_code"

# Matrix keys are numbered row-major with ten columns per row, starting at 1.
# Pins outside the matrix report as individual inputs from 97 up; 81 to 96 are
# not used by the device.
MATRIX_COLUMNS = 10
MATRIX_KEY_MAX = 80
GPI_KEY_MIN = 97
GPI_KEY_MAX = 114

TCA8418BinarySensor = tca8418_ns.class_(
    "TCA8418BinarySensor", binary_sensor.BinarySensorInitiallyOff
)


def _key_code(value: int) -> int:
    value = cv.int_(value)
    if 1 <= value <= MATRIX_KEY_MAX or GPI_KEY_MIN <= value <= GPI_KEY_MAX:
        return value
    raise cv.Invalid(
        f"Key numbers run from 1 to {MATRIX_KEY_MAX} for the key matrix, "
        f"and {GPI_KEY_MIN} to {GPI_KEY_MAX} for individual inputs"
    )


def _validate(config: ConfigType) -> ConfigType:
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
        if len(key) != 1 or not key.isascii():
            raise cv.Invalid("Key must be a single ASCII character", path=[CONF_KEY])
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
            cv.Optional(CONF_KEY_CODE): _key_code,
        }
    ),
    _validate,
)


def _final_validate(config: ConfigType) -> None:
    """Check the key against the keypad, and work out its key number.

    The number is stored back into the configuration so that code generation
    does not have to repeat the lookup, and so the sensor only has to compare
    key numbers at runtime. Final validation is where this happens because it is
    the first point at which the keypad's own configuration can be read.
    """
    full_config = fv.full_config.get()
    keypad_path = full_config.get_path_for_id(config[CONF_KEYPAD_ID])[:-1]
    keypad = full_config.get_config_for_path(keypad_path)
    rows = keypad[CONF_ROWS]
    columns = keypad[CONF_COLUMNS]

    if (row := config.get(CONF_ROW)) is not None:
        col = config[CONF_COL]
        if row >= rows or col >= columns:
            raise cv.Invalid(
                f"Position {row}/{col} is outside the {rows} x {columns} key "
                "matrix of the keypad it belongs to"
            )
        config[CONF_KEY_CODE] = row * MATRIX_COLUMNS + col + 1
        return

    if (key := config.get(CONF_KEY)) is not None:
        keys = keypad.get(CONF_KEYS)
        if keys is None:
            raise cv.Invalid(
                f"'{CONF_KEY}' names a key from the keypad's 'keys', which is not set",
                path=[CONF_KEY],
            )
        if (index := keys.find(key)) < 0:
            raise cv.Invalid(
                f"'{key}' is not one of the keypad's keys", path=[CONF_KEY]
            )
        config[CONF_KEY_CODE] = (
            (index // columns) * MATRIX_COLUMNS + (index % columns) + 1
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config, config[CONF_KEY_CODE])
    keypad = await cg.get_variable(config[CONF_KEYPAD_ID])
    cg.add(keypad.register_listener(var))
