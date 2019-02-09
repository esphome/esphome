import voluptuous as vol

from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SERVERS
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

SNTPComponent = time_.time_ns.class_('SNTPComponent', time_.RealTimeClockComponent)

PLATFORM_SCHEMA = time_.TIME_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SNTPComponent),
    vol.Optional(CONF_SERVERS): vol.All(cv.ensure_list(cv.domain), vol.Length(min=1, max=3)),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_sntp_component()
    sntp = Pvariable(config[CONF_ID], rhs)
    if CONF_SERVERS in config:
        servers = config[CONF_SERVERS]
        servers += [''] * (3 - len(servers))
        add(sntp.set_servers(*servers))

    time_.setup_time(sntp, config)
    setup_component(sntp, config)


BUILD_FLAGS = '-DUSE_SNTP_COMPONENT'
