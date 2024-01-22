import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ina2xx_base, i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@latonita"]
AUTO_LOAD = ["ina2xx_base"]
DEPENDENCIES = ["i2c"]

ina2xx_i2c = cg.esphome_ns.namespace("ina2xx_i2c")
INA2XX_I2C = ina2xx_i2c.class_("INA2XXI2C", ina2xx_base.INA2XX, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(
    ina2xx_base.INA2XX_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(INA2XX_I2C),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x40)),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ina2xx_base.setup_ina2xx(var, config)
    await i2c.register_i2c_device(var, config)
