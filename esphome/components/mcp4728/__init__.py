import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_NUM_CHANNELS
)

DEPENDENCIES = ["i2c"]
MULTI_CONF = True
AUTO_LOAD = ["output"]

mcp4728_ns = cg.esphome_ns.namespace("mcp4728")
MCP4728Output = mcp4728_ns.class_("MCP4728Output", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP4728Output),
            cv.Optional(CONF_NUM_CHANNELS, default=4): cv.int_range(min=1, max=4)
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x60))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    cg.add(var.set_num_channels(config[CONF_NUM_CHANNELS]))