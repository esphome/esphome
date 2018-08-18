import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import time as time_
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_SERVERS
from esphomeyaml.helpers import App, Pvariable

SNTPComponent = time_.time_ns.SNTPComponent

PLATFORM_SCHEMA = time_.TIME_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SNTPComponent),
    vol.Optional(CONF_SERVERS): vol.All(cv.ensure_list, [cv.string], vol.Length(max=3)),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
})


def to_code(config):
    rhs = App.make_sntp_component(*config.get(CONF_SERVERS, []))
    sntp = Pvariable(config[CONF_ID], rhs)

    time_.setup_time(sntp, config)


BUILD_FLAGS = '-DUSE_SNTP_COMPONENT'
