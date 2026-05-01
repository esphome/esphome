import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor"]

CONF_TTP229_ID = "ttp229_id"
ttp229_lsf_ns = cg.esphome_ns.namespace("ttp229_lsf")

TTP229LSFComponent = ttp229_lsf_ns.class_(
    "TTP229LSFComponent", cg.Component, i2c.I2CDevice
)

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TTP229LSFComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x57))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
