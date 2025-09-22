import esphome.codegen as cg
from esphome.components import udp
from esphome.components.logger import LOG_LEVELS, is_log_level
from esphome.components.time import RealTimeClock
from esphome.components.udp import CONF_UDP_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LEVEL, CONF_PORT, CONF_TIME_ID
from esphome.cpp_types import Component, Parented

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["udp", "logger", "time"]

syslog_ns = cg.esphome_ns.namespace("syslog")
Syslog = syslog_ns.class_("Syslog", Component, Parented.template(udp.UDPComponent))

CONF_STRIP = "strip"
CONF_FACILITY = "facility"
CONFIG_SCHEMA = udp.UDP_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Syslog),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(RealTimeClock),
        cv.Optional(CONF_PORT, default=514): cv.port,
        cv.Optional(CONF_LEVEL, default="DEBUG"): is_log_level,
        cv.Optional(CONF_STRIP, default=True): cv.boolean,
        cv.Optional(CONF_FACILITY, default=16): cv.int_range(0, 23),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_UDP_ID])
    time = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(parent.set_broadcast_port(config[CONF_PORT]))
    cg.add(parent.set_should_broadcast())
    level = LOG_LEVELS[config[CONF_LEVEL]]
    var = cg.new_Pvariable(config[CONF_ID], level, time)
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(var.set_strip(config[CONF_STRIP]))
    cg.add(var.set_facility(config[CONF_FACILITY]))
