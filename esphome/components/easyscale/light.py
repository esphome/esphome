import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light, power_supply
from esphome.const import CONF_OUTPUT_ID, CONF_PIN, CONF_POWER_SUPPLY

DEPENDENCIES = ["esp32"]

easyscale_ns = cg.esphome_ns.namespace("easyscale")
EasyScaleLightOutput = easyscale_ns.class_("EasyScale", light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(EasyScaleLightOutput),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
    }
)


async def to_code(config):
    gpio = await cg.gpio_pin_expression(config[CONF_PIN])

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    if CONF_POWER_SUPPLY in config:
        power_supply_ = await cg.get_variable(config[CONF_POWER_SUPPLY])
        cg.add(var.set_power_supply(power_supply_))

    cg.add(var.set_pin(gpio))
