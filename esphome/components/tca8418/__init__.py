from dataclasses import dataclass
import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c, key_provider
from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_ON_KEY
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

DOMAIN = "tca8418"


@dataclass
class TCA8418Data:
    #  How many things are listening for key events, across every keypad. The
    #  firmware sizes its list from this, so it needs the total.
    listener_count: int = 0
    #  More than one keypad can be configured, but the count is only added once.
    final_step_queued: bool = False


def _get_data() -> TCA8418Data:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = TCA8418Data()
    return CORE.data[DOMAIN]


def register_listener() -> None:
    """Count a listener, for the size of the list the firmware keeps."""
    _get_data().listener_count += 1


@coroutine_with_priority(CoroPriority.FINAL)
async def _final_step() -> None:
    if (count := _get_data().listener_count) > 0:
        cg.add_define("TCA8418_LISTENER_COUNT", count)


CODEOWNERS = ["@zebble"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["key_provider", "gpio_expander"]
MULTI_CONF = True

CONF_GPI_EVENTS = "gpi_events"
CONF_KEYPAD_ID = "keypad_id"

# How the device numbers its keys. Matrix keys are numbered row by row with ten
# columns per row, starting at 1, so ROW0/COL0 is 1 and ROW7/COL9 is 80. Pins
# outside the matrix report as individual inputs from 97 up: ROW0..ROW7 are
# 97..104 and COL0..COL9 are 105..114.
MATRIX_COLUMNS = 10
MATRIX_KEY_MAX = 80
GPI_KEY_MIN = 97
GPI_KEY_MAX = 114

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
    rows = config[CONF_ROWS]
    columns = config[CONF_COLUMNS]
    if (rows == 0) != (columns == 0):
        raise cv.Invalid(
            "'rows' and 'columns' must both be set to use the key matrix, "
            "or both left unset to use every pin as an individual input"
        )
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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TCA8418Component),
            # The key matrix occupies ROW0..ROW(rows-1) and COL0..COL(columns-1).
            # Leave both unset to use every pin as an individual input instead.
            cv.Optional(CONF_ROWS, default=0): cv.int_range(min=0, max=8),
            cv.Optional(CONF_COLUMNS, default=0): cv.int_range(min=0, max=10),
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
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    data = _get_data()
    if not data.final_step_queued:
        data.final_step_queued = True
        CORE.add_job(_final_step)

    cg.add(var.set_rows(config[CONF_ROWS]))
    cg.add(var.set_columns(config[CONF_COLUMNS]))
    cg.add(var.set_gpi_events(config[CONF_GPI_EVENTS]))
    if (keys := config.get(CONF_KEYS)) is not None:
        cg.add(var.set_keys(keys))
    if (interrupt_pin := config.get(CONF_INTERRUPT_PIN)) is not None:
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))

    for conf in config.get(CONF_ON_KEY, []):
        await automation.build_callback_automation(
            var, "add_on_key_callback", [(cg.uint8, "x")], conf
        )
