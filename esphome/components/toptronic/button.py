import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, CONF_VALUE
from esphome.types import ConfigType

from . import (
    CONF_DATAPOINT,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_TOPTRONIC_ID,
    CONFIG_SCHEMA_BASE,
    TT_TYPE_OPTIONS,
    TopTronicComponent,
    toptronic,
)

# Range of each encoding; the C++ side clamps to the same limits, so an accepted value is sent unchanged
TYPE_VALUE_RANGES = {
    "U8": (0, 0xFF),
    "U16": (0, 0xFFFF),
    "U32": (0, 0xFFFFFFFF),
    "S8": (-(1 << 7), (1 << 7) - 1),
    "S16": (-(1 << 15), (1 << 15) - 1),
    "S32": (-(1 << 31), (1 << 31) - 1),
    "S64": (-(1 << 63), (1 << 63) - 1),
}

TopTronicButton = toptronic.class_("TopTronicButton", button.Button)


def _validate_value(config: ConfigType) -> ConfigType:
    type_name = next(
        name for name, value in TT_TYPE_OPTIONS.items() if value == config[CONF_TYPE]
    )
    low, high = TYPE_VALUE_RANGES[type_name]
    if not low <= config[CONF_VALUE] <= high:
        raise cv.Invalid(
            f"Value {config[CONF_VALUE]} is outside the {type_name} range "
            f"{low}..{high}",
            path=[CONF_VALUE],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
            cv.Required(CONF_TYPE): cv.enum(TT_TYPE_OPTIONS),
            cv.Required(CONF_VALUE): cv.int_,
        }
    )
    .extend(button.button_schema(TopTronicButton))
    .extend(CONFIG_SCHEMA_BASE),
    _validate_value,
)


async def to_code(config: ConfigType) -> None:
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = await button.new_button(config)

    cg.add(var.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(var.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(var.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_value(config[CONF_VALUE]))

    cg.add(tt.add_input(var))
