from esphomeyaml.components import time as time_
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID
from esphomeyaml.cpp_generator import Pvariable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App

DS1307Time = time_.time_ns.class_('DS1307Time', time_.RealTimeClockComponent)

PLATFORM_SCHEMA = time_.TIME_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(DS1307Time)
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_ds1307_time_component()
    ds1307 = Pvariable(config[CONF_ID], rhs)
    time_.setup_time(ds1307, config)
    setup_component(ds1307, config)


BUILD_FLAGS = '-DUSE_DS1307_TIME'
