import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import cst328_ns
from ..touchscreen import CST328ButtonListener, CST328Touchscreen

CONF_CST328_ID = "cst328_id"

CST328Button = cst328_ns.class_(
    "CST328Button",
    binary_sensor.BinarySensor,
    cg.Component,
    CST328ButtonListener,
    cg.Parented.template(CST328Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(CST328Button).extend(
    {
        cv.GenerateID(CONF_CST328_ID): cv.use_id(CST328Touchscreen),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_CST328_ID])
