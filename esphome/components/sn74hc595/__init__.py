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
    CONF_OUTPUT,
)

DEPENDENCIES = []
MULTI_CONF = True

sn74hc595_ns = cg.esphome_ns.namespace("sn74hc595")

SN74HC595Component = sn74hc595_ns.class_("SN74HC595Component", cg.Component)
SN74HC595GPIOPin = sn74hc595_ns.class_("SN74HC595GPIOPin", cg.GPIOPin)

CONF_SN74HC595 = "sn74hc595"
CONF_LATCH_PIN = "latch_pin"
CONF_OE_PIN = "oe_pin"
CONF_SR_COUNT = "sr_count"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(SN74HC595Component),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_LATCH_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_OE_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_SR_COUNT, default=1): cv.int_range(1, 4),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    data_pin = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data_pin))
    clock_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock_pin))
    latch_pin = await cg.gpio_pin_expression(config[CONF_LATCH_PIN])
    cg.add(var.set_latch_pin(latch_pin))
    if CONF_OE_PIN in config:
        oe_pin = await cg.gpio_pin_expression(config[CONF_OE_PIN])
        cg.add(var.set_oe_pin(oe_pin))
    cg.add(var.set_sr_count(config[CONF_SR_COUNT]))


def _validate_output_mode(value):
    if value is not True:
        raise cv.Invalid("Only output mode is supported")
    return value


SN74HC595_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(SN74HC595GPIOPin),
        cv.Required(CONF_SN74HC595): cv.use_id(SN74HC595Component),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=31),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_OUTPUT, default=True): cv.All(
                    cv.boolean, _validate_output_mode
                ),
            },
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SN74HC595, SN74HC595_PIN_SCHEMA)
async def sn74hc595_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_SN74HC595])
    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    return var
