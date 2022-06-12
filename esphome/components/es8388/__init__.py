import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import i2c
from esphome.const import CONF_ID
from esphome import pins

es8388_ns = cg.esphome_ns.namespace("es8388")
ES8388Component = es8388_ns.class_("ES8388Component", cg.Component, i2c.I2CDevice)

CONF_POWER_PIN = "power_pin"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES8388Component),
            cv.Required(CONF_POWER_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(i2c.i2c_device_schema(0x10))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
    cg.add(var.set_power_pin(pin))
