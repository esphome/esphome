import esphome.codegen as cg
from esphome.components import i2c
from ..ens160_base import to_code_base, cv, CONFIG_SCHEMA_BASE

AUTO_LOAD = ["ens160_base"]
CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]

ens160_ns = cg.esphome_ns.namespace("ens160_i2c")

ENS160I2CComponent = ens160_ns.class_(
    "ENS160I2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x52)
).extend({cv.GenerateID(): cv.declare_id(ENS160I2CComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
