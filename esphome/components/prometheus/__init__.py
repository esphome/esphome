import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INCLUDE_INTERNAL, CONF_NAME, CONF_RELABEL
from esphome.cpp_types import EntityBase

AUTO_LOAD = ["web_server_base"]

prometheus_ns = cg.esphome_ns.namespace("prometheus")
PrometheusHandler = prometheus_ns.class_("PrometheusHandler", cg.Component)

CUSTOMIZED_ENTITY = cv.Schema(
    {
        cv.Optional(CONF_ID): cv.string_strict,
        cv.Optional(CONF_NAME): cv.string_strict,
    },
    cv.has_at_least_one_key,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PrometheusHandler),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
        cv.Optional(CONF_INCLUDE_INTERNAL, default=False): cv.boolean,
        cv.Optional(CONF_RELABEL, default={}): cv.Schema(
            {
                cv.use_id(EntityBase): CUSTOMIZED_ENTITY,
            }
        ),
    },
    cv.only_with_arduino,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    cg.add_define("USE_PROMETHEUS")

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))

    for key, value in config[CONF_RELABEL].items():
        entity = await cg.get_variable(key)
        if CONF_ID in value:
            cg.add(var.add_label_id(entity, value[CONF_ID]))
        if CONF_NAME in value:
            cg.add(var.add_label_name(entity, value[CONF_NAME]))
