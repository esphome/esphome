import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPTIONS, CONF_TYPE
from esphome.types import ConfigType

from . import (
    CONF_DATAPOINT,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_TOPTRONIC_ID,
    CONF_VALUES,
    CONFIG_SCHEMA_BASE,
    TT_TYPE_OPTIONS,
    TopTronicComponent,
    _validate_options_values_lengths,
    toptronic,
)

TopTronicSelect = toptronic.class_(
    "TopTronicSelect", select.Select, cg.PollingComponent
)

# 'values' is written with the configured type, so it has to fit that type's range
TYPE_MAX_VALUE = {"U8": 255, "S8": 127}


def _validate_values_fit_type(config: ConfigType) -> ConfigType:
    """Reject an option value the configured type cannot carry."""
    if (max_value := TYPE_MAX_VALUE.get(config[CONF_TYPE])) is None:
        return config
    for value in config[CONF_VALUES]:
        if value > max_value:
            raise cv.Invalid(
                f"value {value} does not fit type {config[CONF_TYPE]}, whose maximum is {max_value}"
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
            cv.Required(CONF_OPTIONS): cv.All(
                cv.ensure_list(cv.string_strict), cv.Length(min=1)
            ),
            cv.Required(CONF_VALUES): cv.All(
                cv.ensure_list(cv.uint8_t), cv.Length(min=1)
            ),
            cv.Optional(CONF_TYPE, default="U8"): cv.enum(TT_TYPE_OPTIONS),
        }
    )
    .extend(select.select_schema(TopTronicSelect))
    .extend(CONFIG_SCHEMA_BASE),
    _validate_options_values_lengths,
    _validate_values_fit_type,
)


async def to_code(config):
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = await select.new_select(config, options=config[CONF_OPTIONS])
    await cg.register_component(var, config)

    cg.add(var.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(var.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(var.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(var.set_type(config[CONF_TYPE]))

    for i in range(len(config[CONF_OPTIONS])):
        value = config[CONF_VALUES][i]
        text = config[CONF_OPTIONS][i]
        cg.add(var.add_option(value, text))

    cg.add(tt.add_input(var))
