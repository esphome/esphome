import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, rs485
from esphome.const import CONF_ADDRESS, CONF_CLOSE_ACTION, CONF_CLOSE_DURATION, CONF_ID, CONF_OPEN_ACTION, \
    CONF_OPEN_DURATION, CONF_STOP_ACTION

AUTO_LOAD = ['rs485']

dooya_ns = cg.esphome_ns.namespace('dooya')
Dooya = dooya_ns.class_('Dooya', cover.Cover, cg.Component, rs485.RS485Device)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(Dooya),
    cv.Optional(CONF_ADDRESS): cv.uint16_t,

}).extend(cv.COMPONENT_SCHEMA).extend(rs485.RS485_DEVICE_SCHEMA)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield rs485.register_rs485_device(var, config)
    yield cover.register_cover(var, config)