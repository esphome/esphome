import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID
from esphome.components.canbus import CanbusComponent

CODEOWNERS = ["@michaelansel", "@cbialobos"]

espCanBus_ns = cg.esphome_ns.namespace("esp32_canbus")
espCanBus = espCanBus_ns.class_("EspCanBus", CanbusComponent)

CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(espCanBus),
    }
)


def to_code(config):
    rhs = espCanBus.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield canbus.register_canbus(var, config)
