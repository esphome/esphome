import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
MULTI_CONF = True
AUTO_LOAD = ["switch"]
CODEOWNERS = ["@brotherdust"]

m5stack_4relay_ns = cg.esphome_ns.namespace("m5stack_4relay")
M5STACK4RELAYOutput = m5stack_4relay_ns.class_(
    "M5STACK4RELAYSwitchComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5STACK4RELAYOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x26))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
