import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_INCLUDE_INTERNAL,
    CONF_FILTERS,
    CONF_FROM,
    CONF_TO,
)
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.components import web_server_base
from esphome.util import Registry

AUTO_LOAD = ["web_server_base"]

prometheus_ns = cg.esphome_ns.namespace("prometheus")
PrometheusHandler = prometheus_ns.class_("PrometheusHandler", cg.Component)

FILTER_REGISTRY = Registry()
validate_filters = cv.validate_registry("filter", FILTER_REGISTRY)

Filter = prometheus_ns.class_("Filter")
MapFilter = prometheus_ns.class_("MapFilter", Filter)


def validate_mapping(value):
    if not isinstance(value, dict):
        value = cv.string(value)
        if "->" not in value:
            raise cv.Invalid("Mapping must contain '->'")
        a, b = value.split("->", 1)
        value = {CONF_FROM: a.strip(), CONF_TO: b.strip()}

    return cv.Schema(
        {cv.Required(CONF_FROM): cv.string, cv.Required(CONF_TO): cv.string}
    )(value)


@FILTER_REGISTRY.register("map", MapFilter, cv.ensure_list(validate_mapping))
async def map_filter_to_code(config, filter_id):
    map_ = cg.std_ns.class_("map").template(cg.std_string, cg.std_string)
    return cg.new_Pvariable(
        filter_id, map_([(item[CONF_FROM], item[CONF_TO]) for item in config])
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PrometheusHandler),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
        cv.Optional(CONF_INCLUDE_INTERNAL, default=False): cv.boolean,
        cv.Optional(CONF_FILTERS): validate_filters,
    },
    cv.only_with_arduino,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    cg.add_define("USE_PROMETHEUS")

    var = cg.new_Pvariable(config[CONF_ID], paren)

    await cg.register_component(var, config)

    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))

    if config.get(CONF_FILTERS):  # must exist and not be empty
        filters = await build_filters(config[CONF_FILTERS])
        cg.add(var.set_filters(filters))


async def build_filters(config):
    return await cg.build_registry_list(FILTER_REGISTRY, config)
