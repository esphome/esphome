import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, CONF_VALUE

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

TopTronicButton = toptronic.class_(
    "TopTronicButton", button.Button, cg.PollingComponent
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
            cv.Required(CONF_TYPE): cv.enum(TT_TYPE_OPTIONS),
            cv.Required(CONF_VALUE): cv.float_,
        }
    )
    .extend(button.button_schema(TopTronicButton))
    .extend(CONFIG_SCHEMA_BASE)
)


async def to_code(config):
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await button.register_button(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(var.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(var.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_value(config[CONF_VALUE]))

    cg.add(tt.add_input(var))
