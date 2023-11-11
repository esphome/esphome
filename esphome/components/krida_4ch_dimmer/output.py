import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import output, i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]

krida_dimmer = cg.esphome_ns.namespace("KridaDimmer")
KridaDimmer = krida_dimmer.class_("Krida4chDimmer", output.FloatOutput, cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(KridaDimmer),
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