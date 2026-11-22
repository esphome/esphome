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
    TYPE_RANGES,
    TopTronicComponent,
    _request_input,
    _validate_options_values_lengths,
    _value_array,
    toptronic,
)

TopTronicSelect = toptronic.class_(
    "TopTronicSelect", select.Select, cg.PollingComponent
)


def _validate_values_fit_type(config: ConfigType) -> ConfigType:
    """Reject an option value the configured type cannot carry."""
    type_name = str(config[CONF_TYPE])
    low, high = TYPE_RANGES[type_name]
    for value in config[CONF_VALUES]:
        if not low <= value <= high:
            raise cv.Invalid(
                f"value {value} does not fit type {type_name}, whose range is {low}..{high}"
            )
    if len(set(config[CONF_OPTIONS])) != len(config[CONF_OPTIONS]):
        raise cv.Invalid("'options' must be unique for a select")
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


async def to_code(config: ConfigType) -> None:
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=config[CONF_OPTIONS])
    await cg.register_component(var, config)

    cg.add(var.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(var.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(var.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(var.set_type(config[CONF_TYPE]))

    values = _value_array(config[CONF_ID], config[CONF_VALUES])
    cg.add(var.set_values(values, len(config[CONF_VALUES])))
    _request_input(str(tt))
    cg.add(tt.add_input(var))
