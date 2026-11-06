from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
import esphome.config_validation as cv
from esphome.const import CONF_COL, CONF_KEY, CONF_ROW
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_GPI_EVENTS,
    CONF_KEYPAD_ID,
    GPI_KEY_MAX,
    GPI_KEY_MIN,
    KEY,
    KEY_PUBLISHER,
    MATRIX_COLUMNS,
    MATRIX_KEY_MAX,
    PRESSED,
    TCA8418Component,
    _ascii_string,
    _matrix_key,
    tca8418_ns,
)

DEPENDENCIES = ["tca8418"]

CONF_KEY_CODE = "key_code"


def _key_code(value: int) -> int:
    value = cv.int_(value)
    if 1 <= value <= MATRIX_KEY_MAX or GPI_KEY_MIN <= value <= GPI_KEY_MAX:
        return value
    raise cv.Invalid(
        f"Key numbers run from 1 to {MATRIX_KEY_MAX} for the key matrix, "
        f"and {GPI_KEY_MIN} to {GPI_KEY_MAX} for individual inputs"
    )


_POSITION_MSG = f"'{CONF_ROW}' and '{CONF_COL}' must be given together"

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(binary_sensor.BinarySensorInitiallyOff).extend(
        {
            cv.GenerateID(CONF_KEYPAD_ID): cv.use_id(TCA8418Component),
            cv.Inclusive(CONF_ROW, "position", msg=_POSITION_MSG): cv.int_range(
                min=0, max=7
            ),
            cv.Inclusive(CONF_COL, "position", msg=_POSITION_MSG): cv.int_range(
                min=0, max=9
            ),
            cv.Optional(CONF_KEY): cv.All(
                cv.string,
                _ascii_string,
                cv.Length(min=1, max=1, msg="Key must be a single character"),
            ),
            cv.Optional(CONF_KEY_CODE): _key_code,
        }
    ),
    cv.has_exactly_one_key(CONF_ROW, CONF_KEY, CONF_KEY_CODE),
)


def _final_validate(config: ConfigType) -> None:
    """Resolve the key to the number the device reports, checked against the keypad's matrix."""
    keypad = fv.get_declaring_config(config[CONF_KEYPAD_ID])
    rows = keypad.get(CONF_ROWS, 0)
    columns = keypad.get(CONF_COLUMNS, 0)

    if (row := config.get(CONF_ROW)) is not None:
        col = config[CONF_COL]
        if row >= rows or col >= columns:
            raise cv.Invalid(
                f"Position {row}/{col} is outside the {rows} x {columns} key "
                "matrix of the keypad it belongs to",
                path=[CONF_ROW],
            )
        config[CONF_KEY_CODE] = _matrix_key(row, col)
        return

    if (key_code := config.get(CONF_KEY_CODE)) is not None:
        if key_code <= MATRIX_KEY_MAX:
            row, col = divmod(key_code - 1, MATRIX_COLUMNS)
            if row >= rows or col >= columns:
                raise cv.Invalid(
                    f"Key number {key_code} is position {row}/{col}, which is "
                    f"outside the {rows} x {columns} key matrix of the keypad it "
                    "belongs to",
                    path=[CONF_KEY_CODE],
                )
            return
        if not keypad[CONF_GPI_EVENTS]:
            raise cv.Invalid(
                f"Key number {key_code} is an individual input, but the keypad has "
                f"'{CONF_GPI_EVENTS}' turned off",
                path=[CONF_KEY_CODE],
            )
        # 97..104 are ROW0..ROW7 and 105..114 are COL0..COL9. A pin used by the
        # matrix cannot also report on its own.
        offset = key_code - GPI_KEY_MIN
        if offset < 8:
            in_matrix, pin = offset < rows, f"ROW{offset}"
        else:
            in_matrix, pin = offset - 8 < columns, f"COL{offset - 8}"
        if in_matrix:
            raise cv.Invalid(
                f"Key number {key_code} is {pin}, which the key matrix is using, so "
                "it cannot report as an individual input",
                path=[CONF_KEY_CODE],
            )
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
        if keys.count(key) > 1:
            raise cv.Invalid(
                f"'{key}' appears more than once in the keypad's keys, so it does "
                f"not name one key. Use '{CONF_ROW}' and '{CONF_COL}' instead",
                path=[CONF_KEY],
            )
        config[CONF_KEY_CODE] = _matrix_key(*divmod(index, columns))


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    keypad = await cg.get_variable(config[CONF_KEYPAD_ID])
    key_code = config[CONF_KEY_CODE]
    KEY_PUBLISHER.add(
        keypad, tca8418_ns.publish_key(parent_ref(var), KEY, key_code, PRESSED)
    )
