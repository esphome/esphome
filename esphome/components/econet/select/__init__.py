import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_OPTIONS, CONF_ENUM_DATAPOINT

from .. import (
    CONF_ECONET_ID,
    CONF_REQUEST_MOD,
    CONF_REQUEST_ONCE,
    CONF_SRC_ADDRESS,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    econet_ns,
)

DEPENDENCIES = ["econet"]

EconetSelect = econet_ns.class_(
    "EconetSelect", select.Select, cg.Component, EconetClient
)


def ensure_option_map(value):
    cv.check_not_templatable(value)
    options_map_schema = cv.Schema({cv.uint8_t: cv.string_strict})
    value = options_map_schema(value)
    all_values = list(value.keys())
    unique_values = set(value.keys())
    if len(all_values) != len(unique_values):
        raise cv.Invalid("Mapping values must be unique.")
    return value


CONFIG_SCHEMA = (
    select.select_schema(EconetSelect)
    .extend(
        {
            cv.Required(CONF_ENUM_DATAPOINT): cv.string,
            cv.Required(CONF_OPTIONS): ensure_option_map,
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    options_map = config[CONF_OPTIONS]
    var = await select.new_select(config, options=list(options_map.values()))
    await cg.register_component(var, config)
    cg.add(var.set_select_mappings(list(options_map.keys())))
    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))
    cg.add(var.set_request_mod(config[CONF_REQUEST_MOD]))
    cg.add(var.set_request_once(config[CONF_REQUEST_ONCE]))
    cg.add(var.set_select_id(config[CONF_ENUM_DATAPOINT]))
    cg.add(var.set_src_adr(config[CONF_SRC_ADDRESS]))
