import esphome.codegen as cg
from esphome.components import select
from esphome.components.const import CONF_STRING_DATAPOINT
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


def validate_options_shape(config):
    if CONF_STRING_DATAPOINT in config:
        if not isinstance(config[CONF_OPTIONS], list):
            raise cv.Invalid(
                f"With '{CONF_STRING_DATAPOINT}', '{CONF_OPTIONS}' must be a list of strings"
            )
    elif not isinstance(config[CONF_OPTIONS], dict):
        raise cv.Invalid(
            f"'{CONF_OPTIONS}' must be a mapping from datapoint value to option name"
        )
    return config


CONFIG_SCHEMA = cv.All(
    select.select_schema(TuyaSelect)
    .extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_ENUM_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_INT_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_STRING_DATAPOINT): cv.uint8_t,
            cv.Required(CONF_OPTIONS): cv.Any(
                ensure_option_map, cv.ensure_list(cv.string_strict)
            ),
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(
        CONF_ENUM_DATAPOINT, CONF_INT_DATAPOINT, CONF_STRING_DATAPOINT
    ),
    validate_options_shape,
)


async def to_code(config):
    if CONF_STRING_DATAPOINT in config:
        # For string_datapoint, options is a list of strings sent verbatim
        options = config[CONF_OPTIONS]
        mappings = []
    else:
        # For enum/int_datapoint, options is a mapping from datapoint values
        options_map = config[CONF_OPTIONS]
        options = list(options_map.values())
        mappings = list(options_map.keys())

    var = await select.new_select(config, options=options)
    await cg.register_component(var, config)
    cg.add(var.set_select_mappings(mappings))
    await cg.add_variable(var.set_tuya_parent, config[CONF_TUYA_ID])
    if (string_datapoint := config.get(CONF_STRING_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(string_datapoint, False))
        cg.add(var.set_is_string(True))
    if (enum_datapoint := config.get(CONF_ENUM_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(enum_datapoint, False))
    if (int_datapoint := config.get(CONF_INT_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(int_datapoint, True))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
