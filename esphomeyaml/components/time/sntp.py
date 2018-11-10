import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import time as time_
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_SERVERS
from esphomeyaml.helpers import App, Pvariable, add, setup_component

SNTPComponent = time_.time_ns.class_('SNTPComponent', time_.RealTimeClockComponent)

PLATFORM_SCHEMA = time_.TIME_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SNTPComponent),
    vol.Optional(CONF_SERVERS): vol.All(cv.ensure_list, [cv.string], vol.Length(max=3)),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_sntp_component()
    sntp = Pvariable(config[CONF_ID], rhs)
    if CONF_SERVERS in config:
        add(sntp.set_servers(*config[CONF_SERVERS]))

    time_.setup_time(sntp, config)
    setup_component(sntp, config)


BUILD_FLAGS = '-DUSE_SNTP_COMPONENT'
