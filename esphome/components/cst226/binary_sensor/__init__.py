import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import cst226_ns
from ..touchscreen import CST226ButtonListener, CST226Touchscreen

CONF_CST226_ID = "cst226_id"

CST226Button = cst226_ns.class_(
    "CST226Button",
    binary_sensor.BinarySensor,
    cg.Component,
    CST226ButtonListener,
    cg.Parented.template(CST226Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(CST226Button).extend(
    {
        cv.GenerateID(CONF_CST226_ID): cv.use_id(CST226Touchscreen),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_CST226_ID])
