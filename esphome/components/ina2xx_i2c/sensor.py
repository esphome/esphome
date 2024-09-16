import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ina2xx_base, i2c
from esphome.const import CONF_ID, CONF_MODEL

AUTO_LOAD = ["ina2xx_base"]
CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]

ina2xx_i2c = cg.esphome_ns.namespace("ina2xx_i2c")
INA2XX_I2C = ina2xx_i2c.class_("INA2XXI2C", ina2xx_base.INA2XX, i2c.I2CDevice)

INAModel = ina2xx_base.ina2xx_base_ns.enum("INAModel")
INA_MODELS = {
    "INA228": INAModel.INA_228,
    "INA238": INAModel.INA_238,
    "INA237": INAModel.INA_237,
}

CONFIG_SCHEMA = cv.All(
    ina2xx_base.INA2XX_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(INA2XX_I2C),
            cv.Required(CONF_MODEL): cv.enum(INA_MODELS, upper=True),
        }
    ).extend(i2c.i2c_device_schema(0x40)),
    ina2xx_base.validate_model_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ina2xx_base.setup_ina2xx(var, config)
    await i2c.register_i2c_device(var, config)
