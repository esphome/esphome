import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THRESHOLD

CODEOWNERS = ["@tchilov"]
DEPENDENCIES = ["i2c"]

ns2009_ns = cg.esphome_ns.namespace("ns2009")
NS2009Component = ns2009_ns.class_(
    "NS2009Component",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = cv.All(
    touchscreen.touchscreen_schema(calibration_required=True)
    .extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(NS2009Component),
                cv.Optional(CONF_THRESHOLD, default=40): cv.uint8_t,
            }
        )
    )
    .extend(i2c.i2c_device_schema(0x48)),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))
