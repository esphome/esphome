import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import key_provider
from esphome.const import CONF_ID, CONF_PIN

AUTO_LOAD = ["key_provider"]

MULTI_CONF = True

keypad_ns = cg.esphome_ns.namespace("keypad")
Keypad = keypad_ns.class_("Keypad", key_provider.KeyProvider, cg.Component)

CONF_KEYPAD_ID = "keypad_id"
CONF_ROWS = "rows"
CONF_COLUMNS = "columns"
CONF_KEYS = "keys"
CONF_DEBOUNCE_TIME = "debounce_time"
CONF_HAS_DIODES = "has_diodes"


def check_keys(obj):
    if CONF_KEYS in obj:
        if len(obj[CONF_KEYS]) != len(obj[CONF_ROWS]) * len(obj[CONF_COLUMNS]):
            raise cv.Invalid("The number of key codes must equal the number of buttons")
    return obj


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Keypad),
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
        }
    ),
    check_keys,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    pins = []
    for conf in config[CONF_ROWS]:
        pin = await cg.gpio_pin_expression(conf[CONF_PIN])
        pins.append(pin)
    cg.add(var.set_rows(pins))
    pins = []
    for conf in config[CONF_COLUMNS]:
        pin = await cg.gpio_pin_expression(conf[CONF_PIN])
        pins.append(pin)
    cg.add(var.set_columns(pins))
    if CONF_KEYS in config:
        cg.add(var.set_keys(config[CONF_KEYS]))
    cg.add(var.set_debounce_time(config[CONF_DEBOUNCE_TIME]))
    if CONF_HAS_DIODES in config:
        cg.add(var.set_has_diodes(config[CONF_HAS_DIODES]))
