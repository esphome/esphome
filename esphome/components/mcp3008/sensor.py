import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NUMBER, CONF_NAME
from . import mcp3008_ns, MCP3008

DEPENDENCIES = ['mcp3008']

MCP3008Sensor = mcp3008_ns.class_('MCP3008Sensor', sensor.Sensor, cg.PollingComponent)

CONF_MCP3008_ID = 'mcp3008_id'

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(MCP3008Sensor),
    cv.GenerateID(CONF_MCP3008_ID): cv.use_id(MCP3008),
    cv.Required(CONF_NUMBER): cv.int_,
}).extend(cv.polling_component_schema('1s'))


def to_code(config):
    parent = yield cg.get_variable(config[CONF_MCP3008_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent, config[CONF_NAME], config[CONF_NUMBER])
    yield cg.register_component(var, config)
