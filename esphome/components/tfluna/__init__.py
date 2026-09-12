import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@candrews"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

tfluna_ns = cg.esphome_ns.namespace("tfluna")

TFLunaComponent = tfluna_ns.class_("TFLuna", cg.PollingComponent, i2c.I2CDevice)

CONF_TFLUNA_ID = "tfluna_id"
CONF_TIMESTAMP = "timestamp"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TFLunaComponent),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
