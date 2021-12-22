import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_BIT_DEPTH,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_NUM_CHIPS,
)

AUTO_LOAD = ["output"]
my9231_ns = cg.esphome_ns.namespace("my9231")
MY9231OutputComponent = my9231_ns.class_("MY9231OutputComponent", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MY9231OutputComponent),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_NUM_CHANNELS, default=6): cv.int_range(min=3, max=1020),
        cv.Optional(CONF_NUM_CHIPS, default=2): cv.int_range(min=1, max=255),
        cv.Optional(CONF_BIT_DEPTH, default=16): cv.one_of(8, 12, 14, 16, int=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    di = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_pin_di(di))
    dcki = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_pin_dcki(dcki))

    cg.add(var.set_num_channels(config[CONF_NUM_CHANNELS]))
    cg.add(var.set_num_chips(config[CONF_NUM_CHIPS]))
    cg.add(var.set_bit_depth(config[CONF_BIT_DEPTH]))
