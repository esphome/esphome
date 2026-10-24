import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP, CONF_TYPE
from esphome.types import ConfigType

from . import (
    CONF_DATAPOINT,
    CONF_DECIMAL,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_TOPTRONIC_ID,
    CONFIG_SCHEMA_BASE,
    TT_TYPE_OPTIONS,
    TopTronicComponent,
    toptronic,
)

TopTronicNumber = toptronic.class_(
    "TopTronicNumber", number.Number, cg.PollingComponent
)


def _validate_min_max(config: ConfigType) -> ConfigType:
    """Reject a range that cannot be represented as a number entity."""
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid("'min_value' must be smaller than 'max_value'")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
            cv.Required(CONF_TYPE): cv.enum(TT_TYPE_OPTIONS),
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_STEP): cv.positive_float,
            cv.Optional(CONF_DECIMAL, default=0): cv.float_range(min=0),
        }
    )
    .extend(number.number_schema(TopTronicNumber))
    .extend(CONFIG_SCHEMA_BASE),
    _validate_min_max,
)


async def to_code(config):
    divider = pow(10, config[CONF_DECIMAL])
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = await number.new_number(
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
