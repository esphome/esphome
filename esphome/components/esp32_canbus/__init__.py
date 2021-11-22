import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID
from esphome.components.canbus import CanbusComponent

CODEOWNERS = ["@michaelansel", "@cbialobos"]
DEPENDENCIES = ["esp32"]

ns = cg.esphome_ns.namespace("esp32_canbus")
component = ns.class_("OnboardCANController", CanbusComponent)

CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(component),
    }
)


def to_code(config):
    rhs = component.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield canbus.register_canbus(var, config)
