import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c, key_provider
from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
import esphome.config_validation as cv
from esphome.const import CONF_INTERRUPT_PIN, CONF_ON_KEY
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@zebble"]
DEPENDENCIES = ["i2c"]
DOMAIN = "tca8418"
AUTO_LOAD = ["key_provider", "gpio_expander"]
MULTI_CONF = True

# Each keypad keeps its listeners in a fixed size list; the define is the largest count any one keypad needs
_request_listener_slot = cg.slot_counter("TCA8418_LISTENER_COUNT")


async def register_listener(keypad: cg.MockObj, var: cg.MockObj) -> None:
    _request_listener_slot(str(keypad))
    cg.add(keypad.register_listener(var))


CONF_GPI_EVENTS = "gpi_events"
CONF_KEYPAD_ID = "keypad_id"

# Matrix keys are numbered row by row from 1 with ten columns per row; pins outside
# the matrix report as individual inputs 97..114 (ROW0..7, then COL0..9)
MATRIX_COLUMNS = 10
MATRIX_KEY_MAX = 80
GPI_KEY_MIN = 97
GPI_KEY_MAX = 114


def _matrix_key(row: int, col: int) -> int:
    return row * MATRIX_COLUMNS + col + 1


tca8418_ns = cg.esphome_ns.namespace("tca8418")
TCA8418Component = tca8418_ns.class_(
    "TCA8418Component", key_provider.KeyProvider, cg.Component, i2c.I2CDevice
)


def _ascii_string(value: str) -> str:
    """A string of single byte characters, so it lines up with the matrix."""
    value = cv.string_strict(value)
    if not value.isascii():
        raise cv.Invalid(
            "Only ASCII characters can be used, since each key is one character"
        )
    return value


def _validate(config: ConfigType) -> ConfigType:
    rows = config.get(CONF_ROWS, 0)
    columns = config.get(CONF_COLUMNS, 0)
    if (keys := config.get(CONF_KEYS)) is not None:
        if rows == 0:
            raise cv.Invalid(
                "'keys' maps the key matrix, so 'rows' and 'columns' are required",
                path=[CONF_KEYS],
            )
        if len(keys) != rows * columns:
            raise cv.Invalid(
                f"'keys' must have exactly {rows * columns} characters "
                f"({rows} rows x {columns} columns), got {len(keys)}",
                path=[CONF_KEYS],
            )
    # Individual inputs report as 97..114, which are also the codes for the
    # letters 'a' to 'r'. Both reach on_key and key collectors as a number.
    if (
        keys is not None
        and config[CONF_GPI_EVENTS]
        and (
            clashing := sorted(
                {k for k in keys if GPI_KEY_MIN <= ord(k) <= GPI_KEY_MAX}
            )
        )
    ):
        _LOGGER.warning(
            "In '%s', the key(s) %s cannot be told apart from an individual input "
            "on the '%s' and key collector paths, because both are reported as the "
            "same number. Binary sensors are unaffected. Use other characters, or "
            "set '%s: false', if this matters.",
            CONF_KEYS,
            ", ".join(f"'{k}'" for k in clashing),
            CONF_ON_KEY,
            CONF_GPI_EVENTS,
        )
    if rows == 0 and not config[CONF_GPI_EVENTS]:
        raise cv.Invalid(
            "Nothing to report: set 'rows' and 'columns' for a key matrix, "
            "or leave 'gpi_events' enabled to report individual inputs"
        )
    return config


_MATRIX_MSG = (
    "'rows' and 'columns' must both be set to use the key matrix, "
    "or both left unset to use every pin as an individual input"
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TCA8418Component),
            # The key matrix occupies ROW0..ROW(rows-1) and COL0..COL(columns-1).
            # Leave both unset to use every pin as an individual input instead.
            cv.Inclusive(CONF_ROWS, "matrix", msg=_MATRIX_MSG): cv.int_range(
                min=1, max=8
            ),
            cv.Inclusive(CONF_COLUMNS, "matrix", msg=_MATRIX_MSG): cv.int_range(
                min=1, max=10
            ),
            cv.Optional(CONF_KEYS): _ascii_string,
            # Report pins that are not part of the matrix as individual inputs.
            cv.Optional(CONF_GPI_EVENTS, default=True): cv.boolean,
            cv.Optional(CONF_INTERRUPT_PIN): gpio_expander.validate_interrupt_pin,
            cv.Optional(CONF_ON_KEY): automation.validate_automation({}),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x34)),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    if (rows := config.get(CONF_ROWS)) is not None:
        columns = config[CONF_COLUMNS]
        cg.add(var.set_rows(rows))
        cg.add(var.set_columns(columns))
        if (keys := config.get(CONF_KEYS)) is not None:
            # Indexed by key number - 1, with 0 where no key is assigned.
            table = [0] * _matrix_key(rows - 1, columns - 1)
            for index, char in enumerate(keys.encode()):
                table[_matrix_key(*divmod(index, columns)) - 1] = char
            array = cg.shared_progmem_array("tca8418_keys", cg.uint8, table)
            cg.add(var.set_keys(array, len(table)))
    cg.add(var.set_gpi_events(config[CONF_GPI_EVENTS]))
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))

    for conf in config.get(CONF_ON_KEY, []):
        await automation.build_callback_automation(
            var, "add_on_key_callback", [(cg.uint8, "x")], conf
        )
