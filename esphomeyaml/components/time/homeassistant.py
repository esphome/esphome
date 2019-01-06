from esphomeyaml.components import time as time_
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID
from esphomeyaml.cpp_generator import Pvariable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App


DEPENDENCIES = ['api']

HomeAssistantTime = time_.time_ns.class_('HomeAssistantTime', time_.RealTimeClockComponent)

PLATFORM_SCHEMA = time_.TIME_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HomeAssistantTime),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_homeassistant_time_component()
    ha_time = Pvariable(config[CONF_ID], rhs)
    time_.setup_time(ha_time, config)
    setup_component(ha_time, config)


BUILD_FLAGS = '-DUSE_HOMEASSISTANT_TIME'
