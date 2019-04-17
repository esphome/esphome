import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_COMPONENT_ID, CONF_NAME, CONF_PAGE_ID, CONF_ID
from . import nextion_ns
from .display import Nextion

DEPENDENCIES = ['display']

CONF_NEXTION_ID = 'nextion_id'

NextionTouchComponent = nextion_ns.class_('NextionTouchComponent', binary_sensor.BinarySensor)

CONFIG_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(NextionTouchComponent),
    cv.GenerateID(CONF_NEXTION_ID): cv.use_variable_id(Nextion),

    cv.Required(CONF_PAGE_ID): cv.uint8_t,
    cv.Required(CONF_COMPONENT_ID): cv.uint8_t,
}))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_NEXTION_ID])
    rhs = hub.make_touch_component(config[CONF_NAME], config[CONF_PAGE_ID],
                                   config[CONF_COMPONENT_ID])
    var = cg.Pvariable(config[CONF_ID], rhs)
    yield binary_sensor.register_binary_sensor(var, config)
