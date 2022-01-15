import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ENABLE_TIME, CONF_ID, CONF_KEEP_ON_TIME, CONF_PIN

CODEOWNERS = ["@esphome/core"]
power_supply_ns = cg.esphome_ns.namespace("power_supply")
PowerSupply = power_supply_ns.class_("PowerSupply", cg.Component)
MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(PowerSupply),
        cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
        cv.Optional(
            CONF_ENABLE_TIME, default="20ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_KEEP_ON_TIME, default="10s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_enable_time(config[CONF_ENABLE_TIME]))
    cg.add(var.set_keep_on_time(config[CONF_KEEP_ON_TIME]))

    cg.add_define("USE_POWER_SUPPLY")
