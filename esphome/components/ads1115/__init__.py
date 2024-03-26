import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

ads1115_ns = cg.esphome_ns.namespace("ads1115")
ADS1115Component = ads1115_ns.class_("ADS1115Component", cg.Component, i2c.I2CDevice)

CONF_CONTINUOUS_MODE = "continuous_mode"
CONF_ADS1115_ID = "ads1115_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS1115Component),
            cv.Optional(CONF_CONTINUOUS_MODE, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(None))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_continuous_mode(config[CONF_CONTINUOUS_MODE]))
