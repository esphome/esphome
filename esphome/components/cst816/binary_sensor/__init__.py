import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import cst816_ns
from ..touchscreen import CST816Touchscreen, CST816ButtonListener

CONF_CST816_ID = "cst816_id"

CST816Button = cst816_ns.class_(
    "CST816Button",
    binary_sensor.BinarySensor,
    cg.Component,
    CST816ButtonListener,
    cg.Parented.template(CST816Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(CST816Button).extend(
    {
        cv.GenerateID(CONF_CST816_ID): cv.use_id(CST816Touchscreen),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_CST816_ID])
