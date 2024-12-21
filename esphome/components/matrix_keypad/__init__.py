from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import key_provider
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_KEY, CONF_PIN, CONF_TRIGGER_ID

CODEOWNERS = ["@ssieb"]

AUTO_LOAD = ["key_provider"]

MULTI_CONF = True

matrix_keypad_ns = cg.esphome_ns.namespace("matrix_keypad")
MatrixKeypad = matrix_keypad_ns.class_(
    "MatrixKeypad", key_provider.KeyProvider, cg.Component
)
MatrixKeyTrigger = matrix_keypad_ns.class_(
    "MatrixKeyTrigger", automation.Trigger.template(cg.uint8)
)

CONF_KEYPAD_ID = "keypad_id"
CONF_ROWS = "rows"
CONF_COLUMNS = "columns"
CONF_KEYS = "keys"
CONF_DEBOUNCE_TIME = "debounce_time"
CONF_HAS_DIODES = "has_diodes"
CONF_HAS_PULLDOWNS = "has_pulldowns"


def check_keys(obj):
    if CONF_KEYS in obj:
        if len(obj[CONF_KEYS]) != len(obj[CONF_ROWS]) * len(obj[CONF_COLUMNS]):
            raise cv.Invalid("The number of key codes must equal the number of buttons")
    return obj


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
            cv.Optional(CONF_ON_KEY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MatrixKeyTrigger),
                }
            ),
        }
    ),
    check_keys,
)


async def to_code(config):
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
    if CONF_KEYS in config:
        cg.add(var.set_keys(config[CONF_KEYS]))
    cg.add(var.set_debounce_time(config[CONF_DEBOUNCE_TIME]))
    if CONF_HAS_DIODES in config:
        cg.add(var.set_has_diodes(config[CONF_HAS_DIODES]))
    if CONF_HAS_PULLDOWNS in config:
        cg.add(var.set_has_pulldowns(config[CONF_HAS_PULLDOWNS]))
    for conf in config.get(CONF_ON_KEY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_key_trigger(trigger))
        await automation.build_automation(trigger, [(cg.uint8, "x")], conf)
