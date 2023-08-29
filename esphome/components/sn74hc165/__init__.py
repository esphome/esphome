import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_NUMBER,
    CONF_INVERTED,
    CONF_DATA_PIN,
    CONF_CLOCK_PIN,
    CONF_INPUT,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = []
MULTI_CONF = True

sn74hc165_ns = cg.esphome_ns.namespace("sn74hc165")

SN74HC165Component = sn74hc165_ns.class_("SN74HC165Component", cg.Component)
SN74HC165GPIOPin = sn74hc165_ns.class_(
    "SN74HC165GPIOPin", cg.GPIOPin, cg.Parented.template(SN74HC165Component)
)

CONF_SN74HC165 = "sn74hc165"
CONF_LOAD_PIN = "load_pin"
CONF_CLOCK_INHIBIT_PIN = "clock_inhibit_pin"
CONF_SR_COUNT = "sr_count"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(SN74HC165Component),
        cv.Required(CONF_DATA_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_LOAD_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_CLOCK_INHIBIT_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_SR_COUNT, default=1): cv.int_range(min=1, max=256),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    data_pin = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data_pin))
    clock_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock_pin))
    load_pin = await cg.gpio_pin_expression(config[CONF_LOAD_PIN])
    cg.add(var.set_load_pin(load_pin))
    if CONF_CLOCK_INHIBIT_PIN in config:
        clock_inhibit_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_INHIBIT_PIN])
        cg.add(var.set_clock_inhibit_pin(clock_inhibit_pin))

    cg.add(var.set_sr_count(config[CONF_SR_COUNT]))


def _validate_input_mode(value):
    if value is not True:
        raise cv.Invalid("Only input mode is supported")
    return value


SN74HC165_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(SN74HC165GPIOPin),
        cv.Required(CONF_SN74HC165): cv.use_id(SN74HC165Component),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=2048, max_included=False),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=True): cv.All(
                    cv.boolean, _validate_input_mode
                ),
            },
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SN74HC165, SN74HC165_PIN_SCHEMA)
async def sn74hc165_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_SN74HC165])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    return var
