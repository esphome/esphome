import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import chsc5816_ns
from ..touchscreen import CHSC5816ButtonListener, CHSC5816Touchscreen

CONF_CHSC5816_ID = "chsc5816_id"

CHSC5816Button = chsc5816_ns.class_(
    "CHSC5816Button",
    binary_sensor.BinarySensor,
    cg.Component,
    CHSC5816ButtonListener,
    cg.Parented.template(CHSC5816Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(CHSC5816Button).extend(
    {
        cv.GenerateID(CONF_CHSC5816_ID): cv.use_id(CHSC5816Touchscreen),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_CHSC5816_ID])
