import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_INDEX

from .. import tt21100_ns
from ..touchscreen import TT21100ButtonListener, TT21100Touchscreen

CONF_TT21100_ID = "tt21100_id"

TT21100Button = tt21100_ns.class_(
    "TT21100Button",
    binary_sensor.BinarySensor,
    cg.Component,
    TT21100ButtonListener,
    cg.Parented.template(TT21100Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(TT21100Button).extend(
    {
        cv.GenerateID(CONF_TT21100_ID): cv.use_id(TT21100Touchscreen),
        cv.Required(CONF_INDEX): cv.int_range(min=0, max=3),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_TT21100_ID])
    cg.add(var.set_index(config[CONF_INDEX]))
