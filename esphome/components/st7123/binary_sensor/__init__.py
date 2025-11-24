import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_INDEX

from .. import st7123_ns
from ..touchscreen import ST7123ButtonListener, ST7123Touchscreen

CONF_ST7123_ID = "st7123_id"

ST7123Button = st7123_ns.class_(
    "ST7123Button",
    binary_sensor.BinarySensor,
    cg.Component,
    ST7123ButtonListener,
    cg.Parented.template(ST7123Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(ST7123Button).extend(
    {
        cv.GenerateID(CONF_ST7123_ID): cv.use_id(ST7123Touchscreen),
        cv.Optional(CONF_INDEX, default=0): cv.int_range(min=0, max=3),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ST7123_ID])
    cg.add(var.set_index(config[CONF_INDEX]))
