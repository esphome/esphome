from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import key_provider
from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_KEY, CONF_PIN
from esphome.types import ConfigType

CODEOWNERS = ["@ssieb"]
DOMAIN = "matrix_keypad"

AUTO_LOAD = ["key_provider"]

MULTI_CONF = True

matrix_keypad_ns = cg.esphome_ns.namespace("matrix_keypad")
MatrixKeypad = matrix_keypad_ns.class_(
    "MatrixKeypad", key_provider.KeyProvider, cg.Component
)

CONF_KEYPAD_ID = "keypad_id"
CONF_DEBOUNCE_TIME = "debounce_time"
CONF_HAS_DIODES = "has_diodes"
CONF_HAS_PULLDOWNS = "has_pulldowns"


def check_keys(obj: ConfigType) -> ConfigType:
    for ch in obj.get(CONF_KEYS, ""):
        if not ch.isascii():
            # Each key is reported as one byte, so only ASCII characters can be key codes
            raise cv.Invalid(f"Key code {ch!r} is not an ASCII character")
    if CONF_KEYS in obj and len(obj[CONF_KEYS]) != len(obj[CONF_ROWS]) * len(
        obj[CONF_COLUMNS]
    ):
        raise cv.Invalid("The number of key codes must equal the number of buttons")
    return obj


_AUTOMATION_LISTS = (
    automation.AutomationList(
        CONF_ON_KEY,
        "add_on_key_automation",
        "MATRIX_KEYPAD_ON_KEY_COUNT",
        [(cg.uint8, "x")],
    ),
)

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MatrixKeypad),
            cv.Required(CONF_ROWS): cv.All(
                cv.ensure_list({cv.Required(CONF_PIN): pins.gpio_output_pin_schema}),
                cv.Length(min=1),
            ),
            cv.Required(CONF_COLUMNS): cv.All(
                cv.ensure_list({cv.Required(CONF_PIN): pins.gpio_input_pin_schema}),
                cv.Length(min=1),
            ),
            cv.Optional(CONF_KEYS): cv.string,
            cv.Optional(CONF_DEBOUNCE_TIME, default=1): cv.int_range(min=1, max=100),
            cv.Optional(CONF_HAS_DIODES): cv.boolean,
            cv.Optional(CONF_HAS_PULLDOWNS): cv.boolean,
            cv.Optional(CONF_ON_KEY): automation.validate_automation(),
        }
    ),
    check_keys,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    row_pins = []
    for conf in config[CONF_ROWS]:
        pin = await cg.gpio_pin_expression(conf[CONF_PIN])
        row_pins.append(pin)
    cg.add(var.set_rows(row_pins))
    col_pins = []
    for conf in config[CONF_COLUMNS]:
        pin = await cg.gpio_pin_expression(conf[CONF_PIN])
        col_pins.append(pin)
    cg.add(var.set_columns(col_pins))
    if (keys := config.get(CONF_KEYS)) is not None:
        table = cg.shared_progmem_array(
            "matrix_keypad_keys", cg.uint8, list(keys.encode())
        )
        cg.add(var.set_keys(table))
    cg.add(var.set_debounce_time(config[CONF_DEBOUNCE_TIME]))
    cg.add_optional(var.set_has_diodes, config.get(CONF_HAS_DIODES))
    cg.add_optional(var.set_has_pulldowns, config.get(CONF_HAS_PULLDOWNS))
    await automation.build_automation_lists(var, config, _AUTOMATION_LISTS)
