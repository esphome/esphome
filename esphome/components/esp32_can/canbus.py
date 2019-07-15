import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID


esp32_can_ns = cg.esphome_ns.namespace('esp32_can')
esp32_can = esp32_can_ns.class_('ESP32Can', cg.Component, canbus.CanbusComponent)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(esp32_can),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = esp32_can.new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    yield cg.register_component(var, config)

