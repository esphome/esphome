import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv

from ..xensiv_pasco2_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["xensiv_pasco2_base"]
DEPENDENCIES = ["i2c"]

xensiv_pasco2_ns = cg.esphome_ns.namespace("xensiv_pasco2_i2c")
XENSIVPASCO2I2CComponent = xensiv_pasco2_ns.class_(
    "XensivPasCO2I2CComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x28)
).extend({cv.GenerateID(): cv.declare_id(XENSIVPASCO2I2CComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
