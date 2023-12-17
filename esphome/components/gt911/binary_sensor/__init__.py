import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_INDEX

from .. import gt911_ns
from ..touchscreen import GT911Touchscreen, GT911ButtonListener

CONF_GT911_ID = "gt911_id"

GT911Button = gt911_ns.class_(
    "GT911Button",
    binary_sensor.BinarySensor,
    cg.Component,
    GT911ButtonListener,
    cg.Parented.template(GT911Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(GT911Button).extend(
    {
        cv.GenerateID(CONF_GT911_ID): cv.use_id(GT911Touchscreen),
        cv.Optional(CONF_INDEX, default=0): cv.int_range(min=0, max=3),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_GT911_ID])
    cg.add(var.set_index(config[CONF_INDEX]))
