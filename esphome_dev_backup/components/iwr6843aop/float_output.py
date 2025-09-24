import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["iwr6843aop"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(number.Number),
        cv.Optional("USER 1 X", default=0.0): cv.float_,
        cv.Optional("USER 1 Y", default=0.0): cv.float_,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_float_output(1, config["USER 1 X"]))
    cg.add(var.set_float_output(2, config["USER 1 Y"]))
