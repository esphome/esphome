import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CONF_TOUCH_THRESHOLD = "touch_threshold"
CONF_RELEASE_THRESHOLD = "release_threshold"
CONF_TOUCH_DEBOUNCE = "touch_debounce"
CONF_RELEASE_DEBOUNCE = "release_debounce"

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor"]

mpr121_ns = cg.esphome_ns.namespace("mpr121")
CONF_MPR121_ID = "mpr121_id"
MPR121Component = mpr121_ns.class_("MPR121Component", cg.Component, i2c.I2CDevice)

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MPR121Component),
            cv.Optional(CONF_RELEASE_DEBOUNCE, default=0): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TOUCH_DEBOUNCE, default=0): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TOUCH_THRESHOLD, default=0x0B): cv.int_range(
                min=0x05, max=0x30
            ),
            cv.Optional(CONF_RELEASE_THRESHOLD, default=0x06): cv.int_range(
                min=0x05, max=0x30
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x5A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_touch_debounce(config[CONF_TOUCH_DEBOUNCE]))
    cg.add(var.set_release_debounce(config[CONF_RELEASE_DEBOUNCE]))
    cg.add(var.set_touch_threshold(config[CONF_TOUCH_THRESHOLD]))
    cg.add(var.set_release_threshold(config[CONF_RELEASE_THRESHOLD]))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
