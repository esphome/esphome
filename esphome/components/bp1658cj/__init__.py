import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
)

CODEOWNERS = ["@Cossid"]
MULTI_CONF = True

CONF_MAX_POWER_COLOR_CHANNELS = "max_power_color_channels"
CONF_MAX_POWER_WHITE_CHANNELS = "max_power_white_channels"

AUTO_LOAD = ["output"]
bp1658cj_ns = cg.esphome_ns.namespace("bp1658cj")
BP1658CJ = bp1658cj_ns.class_("BP1658CJ", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BP1658CJ),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_MAX_POWER_COLOR_CHANNELS, default=2): cv.int_range(
            min=0, max=15
        ),
        cv.Optional(CONF_MAX_POWER_WHITE_CHANNELS, default=4): cv.int_range(
            min=0, max=15
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data))
    clock = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock))
    cg.add(var.set_max_power_color_channels(config[CONF_MAX_POWER_COLOR_CHANNELS]))
    cg.add(var.set_max_power_white_channels(config[CONF_MAX_POWER_WHITE_CHANNELS]))
