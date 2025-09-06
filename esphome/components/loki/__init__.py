import esphome.codegen as cg
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
from esphome.components.logger import LOG_LEVELS, is_log_level
from esphome.components.time import RealTimeClock
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LEVEL, CONF_PORT, CONF_TIME_ID, CONF_URL
from esphome.cpp_types import Component, Parented

CODEOWNERS = ["@jzucker2"]

AUTO_LOAD = ["json"]
DEPENDENCIES = ["network", "logger", "time", "http_request"]

loki_ns = cg.esphome_ns.namespace("loki")
Loki = loki_ns.class_("Loki", Component, Parented.template(HttpRequestComponent))

CONF_LOKI_ID = "loki_id"
CONF_STRIP = "strip"
CONF_ENABLED = "enabled"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Loki),
        cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(RealTimeClock),
        cv.Required(CONF_URL): cv.url,
        cv.Optional(CONF_PORT, default=3100): cv.port,
        cv.Optional(CONF_LEVEL, default="DEBUG"): is_log_level,
        cv.Optional(CONF_STRIP, default=True): cv.boolean,
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
    time = await cg.get_variable(config[CONF_TIME_ID])
    level = LOG_LEVELS[config[CONF_LEVEL]]
    var = cg.new_Pvariable(config[CONF_ID], level, time)
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(var.set_strip(config[CONF_STRIP]))
    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_enabled(config[CONF_ENABLED]))
