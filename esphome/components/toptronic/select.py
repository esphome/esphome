import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPTIONS, CONF_TYPE

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

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
            cv.Required(CONF_OPTIONS): cv.All(
                cv.ensure_list(cv.string_strict), cv.Length(min=1)
            ),
            cv.Required(CONF_VALUES): cv.All(cv.ensure_list(cv.int_), cv.Length(min=1)),
            cv.Optional(CONF_TYPE, default="U8"): cv.enum(TT_TYPE_OPTIONS),
        }
    )
    .extend(select.select_schema(TopTronicSelect))
    .extend(CONFIG_SCHEMA_BASE),
    _validate_options_values_lengths,
)


async def new_select(config, *, options: list[str]):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=options)
    return var


async def to_code(config):
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = await new_select(config, options=config[CONF_OPTIONS])
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
