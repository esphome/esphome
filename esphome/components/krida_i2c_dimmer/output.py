import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import output, i2c
from esphome.const import CONF_ID, CONF_CHANNEL

DEPENDENCIES = ["i2c"]

krida_dimmer = cg.esphome_ns.namespace("krida_i2c_dimmer")
KridaDimmer = krida_dimmer.class_(
    "KridaI2CDimmer", output.FloatOutput, cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(KridaDimmer),
            cv.Required(CONF_CHANNEL): cv.hex_int_range(0x80, 0x85),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x27))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await output.register_output(var, config)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
