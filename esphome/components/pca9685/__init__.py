import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_FREQUENCY, CONF_ID

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

pca9685_ns = cg.esphome_ns.namespace("pca9685")
PCA9685Output = pca9685_ns.class_("PCA9685Output", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PCA9685Output),
            cv.Required(CONF_FREQUENCY): cv.All(
                cv.frequency, cv.Range(min=23.84, max=1525.88)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_FREQUENCY])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
