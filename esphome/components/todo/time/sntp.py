from esphome.components import time as time_
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SERVERS


SNTPComponent = time_.time_ns.class_('SNTPComponent', time_.RealTimeClockComponent)

PLATFORM_SCHEMA = time_.TIME_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SNTPComponent),
    cv.Optional(CONF_SERVERS): cv.All(cv.ensure_list(cv.domain), cv.Length(min=1, max=3)),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_sntp_component()
    sntp = Pvariable(config[CONF_ID], rhs)
    if CONF_SERVERS in config:
        servers = config[CONF_SERVERS]
        servers += [''] * (3 - len(servers))
        cg.add(sntp.set_servers(*servers))

    time_.setup_time(sntp, config)
    register_component(sntp, config)


BUILD_FLAGS = '-DUSE_SNTP_COMPONENT'
