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
    TYPE_RANGES,
    TopTronicComponent,
    register_input,
    toptronic,
)

TopTronicNumber = toptronic.class_("TopTronicNumber", number.Number)


def _validate_range(config: ConfigType) -> ConfigType:
    """Reject a range the wire type cannot carry, or one the entity cannot represent."""
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid("'min_value' must be smaller than 'max_value'")
    # config[CONF_TYPE] holds the option name; its codegen expression cannot be compared
    type_name = str(config[CONF_TYPE])
    low, high = TYPE_RANGES[type_name]
    if config[CONF_MIN_VALUE] < low or config[CONF_MAX_VALUE] > high:
        raise cv.Invalid(
            f"'min_value'..'max_value' must fit type {type_name}, whose range is "
            f"{low}..{high}"
        )
    if config[CONF_STEP] > high - low:
        raise cv.Invalid(f"'step' must not exceed the range of type {type_name}")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
            cv.Required(CONF_TYPE): cv.enum(TT_TYPE_OPTIONS),
            # Raw device units: min_value/max_value/step are divided by 10**decimal for the entity
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_STEP): cv.positive_not_null_float,
            cv.Optional(CONF_DECIMAL, default=0): cv.int_range(min=0, max=6),
        }
    )
    .extend(number.number_schema(TopTronicNumber))
    .extend(CONFIG_SCHEMA_BASE),
    _validate_range,
)


async def to_code(config: ConfigType) -> None:
    divider = pow(10, config[CONF_DECIMAL])
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE] / divider,
        max_value=config[CONF_MAX_VALUE] / divider,
        step=config[CONF_STEP] / divider,
    )

    cg.add(var.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(var.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(var.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_multiplier(divider))

    cg.add(register_input(tt, var))
