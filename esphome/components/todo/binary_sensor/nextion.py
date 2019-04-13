from esphome.components import binary_sensor, display
from esphome.components.display.nextion import Nextion
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_COMPONENT_ID, CONF_NAME, CONF_PAGE_ID
DEPENDENCIES = ['display']

CONF_NEXTION_ID = 'nextion_id'

NextionTouchComponent = display.display_ns.class_('NextionTouchComponent',
                                                  binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(NextionTouchComponent),
    cv.Required(CONF_PAGE_ID): cv.uint8_t,
    cv.Required(CONF_COMPONENT_ID): cv.uint8_t,
    cv.GenerateID(CONF_NEXTION_ID): cv.use_variable_id(Nextion)
}))


def to_code(config):
    hub = yield get_variable(config[CONF_NEXTION_ID])
    rhs = hub.make_touch_component(config[CONF_NAME], config[CONF_PAGE_ID],
                                   config[CONF_COMPONENT_ID])
    binary_sensor.register_binary_sensor(rhs, config)
