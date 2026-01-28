import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENUM_DATAPOINT,
    CONF_INT_DATAPOINT,
    CONF_OPTIMISTIC,
    CONF_OPTIONS,
)

from .. import CONF_TUYA_ID, Tuya, tuya_ns

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@bearpawmaxim"]

TuyaSelect = tuya_ns.class_("TuyaSelect", select.Select, cg.Component)


def ensure_option_map(value):
    cv.check_not_templatable(value)
    option = cv.All(cv.int_range(0, 2**8 - 1))
    mapping = cv.All(cv.string_strict)
    options_map_schema = cv.Schema({option: mapping})
    value = options_map_schema(value)

    all_values = list(value.keys())
    unique_values = set(value.keys())
    if len(all_values) != len(unique_values):
        raise cv.Invalid("Mapping values must be unique.")

    return value


CONFIG_SCHEMA = cv.All(
    select.select_schema(TuyaSelect)
    .extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_ENUM_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_INT_DATAPOINT): cv.uint8_t,
            cv.Required(CONF_OPTIONS): ensure_option_map,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_ENUM_DATAPOINT, CONF_INT_DATAPOINT),
)


async def to_code(config):
    options_map = config[CONF_OPTIONS]
    var = await select.new_select(config, options=list(options_map.values()))
    await cg.register_component(var, config)
    cg.add(var.set_select_mappings(list(options_map.keys())))
    parent = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(parent))
    if (enum_datapoint := config.get(CONF_ENUM_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(enum_datapoint, False))
    if (int_datapoint := config.get(CONF_INT_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(int_datapoint, True))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
