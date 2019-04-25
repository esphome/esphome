from esphome.components import time as time_
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from .. import homeassistant_ns

DEPENDENCIES = ['api']

HomeassistantTime = homeassistant_ns.class_('HomeassistantTime', time_.RealTimeClock)

CONFIG_SCHEMA = time_.TIME_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(HomeassistantTime),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield time_.register_time(var, config)
    yield cg.register_component(var, config)
    cg.add_define('USE_HOMEASSISTANT_TIME')
