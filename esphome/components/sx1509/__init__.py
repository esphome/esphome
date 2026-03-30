from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c, key_provider
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_ON_KEY,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_TRIGGER_ID,
)

CONF_KEYPAD = "keypad"
CONF_KEYS = "keys"
CONF_KEY_ROWS = "key_rows"
CONF_KEY_COLUMNS = "key_columns"
CONF_SLEEP_TIME = "sleep_time"
CONF_SCAN_TIME = "scan_time"
CONF_DEBOUNCE_TIME = "debounce_time"
CONF_SX1509_ID = "sx1509_id"

AUTO_LOAD = ["key_provider", "gpio_expander"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

sx1509_ns = cg.esphome_ns.namespace("sx1509")

SX1509Component = sx1509_ns.class_(
    "SX1509Component", cg.Component, i2c.I2CDevice, key_provider.KeyProvider
)
SX1509GPIOPin = sx1509_ns.class_("SX1509GPIOPin", cg.GPIOPin)
SX1509KeyTrigger = sx1509_ns.class_(
    "SX1509KeyTrigger", automation.Trigger.template(cg.uint8)
)


def check_keys(config):
    if (
        CONF_KEYS in config
        and len(config[CONF_KEYS]) != config[CONF_KEY_ROWS] * config[CONF_KEY_COLUMNS]
    ):
        raise cv.Invalid(
            "The number of key codes must equal the number of rows * columns"
        )
    return config


KEYPAD_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_KEY_ROWS): cv.int_range(min=2, max=8),
            cv.Required(CONF_KEY_COLUMNS): cv.int_range(min=1, max=8),
            cv.Optional(CONF_SLEEP_TIME): cv.int_range(min=128, max=8192),
            cv.Optional(CONF_SCAN_TIME): cv.int_range(min=1, max=128),
            cv.Optional(CONF_DEBOUNCE_TIME): cv.int_range(min=1, max=64),
            cv.Optional(CONF_KEYS): cv.string,
            cv.Optional(CONF_ON_KEY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SX1509KeyTrigger),
                }
            ),
        }
    ),
    check_keys,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SX1509Component),
            cv.Optional(CONF_KEYPAD): cv.Schema(KEYPAD_SCHEMA),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x3E))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if conf := config.get(CONF_KEYPAD):
        cg.add(var.set_rows_cols(conf[CONF_KEY_ROWS], conf[CONF_KEY_COLUMNS]))
        if (
            CONF_SLEEP_TIME in conf
            and CONF_SCAN_TIME in conf
            and CONF_DEBOUNCE_TIME in conf
        ):
            cg.add(var.set_sleep_time(conf[CONF_SLEEP_TIME]))
            cg.add(var.set_scan_time(conf[CONF_SCAN_TIME]))
            cg.add(var.set_debounce_time(conf[CONF_DEBOUNCE_TIME]))
        if keys := conf.get(CONF_KEYS):
            cg.add(var.set_keys(keys))
        for tconf in conf.get(CONF_ON_KEY, []):
            trigger = cg.new_Pvariable(tconf[CONF_TRIGGER_ID])
            cg.add(var.register_key_trigger(trigger))
            await automation.build_automation(trigger, [(cg.uint8, "x")], tconf)


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLUP] and not value[CONF_INPUT]:
        raise cv.Invalid("Pullup only available with input")
    if value[CONF_PULLDOWN] and not value[CONF_INPUT]:
        raise cv.Invalid("Pulldown only available with input")
    if value[CONF_PULLUP] and value[CONF_PULLDOWN]:
        raise cv.Invalid("Can only have one of pullup or pulldown")
    if value[CONF_OPEN_DRAIN] and not value[CONF_OUTPUT]:
        raise cv.Invalid("Open drain available only with output")
    return value


CONF_SX1509 = "sx1509"
SX1509_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(SX1509GPIOPin),
        cv.Required(CONF_SX1509): cv.use_id(SX1509Component),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=15),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_PULLUP, default=False): cv.boolean,
                cv.Optional(CONF_PULLDOWN, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
                cv.Optional(CONF_OPEN_DRAIN, default=False): cv.boolean,
            },
            validate_mode,
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SX1509, SX1509_PIN_SCHEMA)
async def sx1509_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_SX1509])
    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
