import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    CONF_TYPE,
)

from . import (
    toptronic,
    CONF_TT_ID,
    TopTronicComponent,
    CONFIG_SCHEMA_BASE,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_DATAPOINT,
    TT_TYPE_OPTIONS,
    CONF_DECIMAL,
)

TopTronicNumber = toptronic.class_(
    "TopTronicNumber", number.Number, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_TT_ID): cv.use_id(TopTronicComponent),
    cv.Required(CONF_TYPE): cv.enum(TT_TYPE_OPTIONS),
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Required(CONF_MIN_VALUE): cv.float_,
    cv.Required(CONF_STEP): cv.positive_float,
    cv.Optional(CONF_DECIMAL, default=0): cv.float_range(min=0),
}).extend(number.number_schema(
    TopTronicNumber
)).extend(CONFIG_SCHEMA_BASE)


async def new_number(config, *, min_value: float, max_value: float, step: float):
    var = cg.new_Pvariable(config[CONF_ID])
    await number.register_number(
        var, config, min_value=min_value, max_value=max_value, step=step
    )
    return var


async def to_code(config):
    divider = pow(10, config[CONF_DECIMAL])
    tt = await cg.get_variable(config[CONF_TT_ID])
    var = await new_number(
        config,
        min_value=config[CONF_MIN_VALUE] / divider,
        max_value=config[CONF_MAX_VALUE] / divider,
        step=config[CONF_STEP] / divider,
    )
    await cg.register_component(var, config)

    cg.add(var.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(var.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(var.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_multiplier(divider))

    cg.add(tt.add_input(var))
