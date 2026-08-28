import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_TYPE

from . import (
    toptronic,
    CONF_TT_ID,
    TopTronicComponent,
    CONFIG_SCHEMA_BASE,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_DATAPOINT,
    TT_TYPE_OPTIONS,
)

CONF_VALUE = "value"

TopTronicButton = toptronic.class_(
    "TopTronicButton", button.Button, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_TT_ID): cv.use_id(TopTronicComponent),
    cv.Required(CONF_TYPE): cv.enum(TT_TYPE_OPTIONS),
    cv.Required(CONF_VALUE): cv.float_,
}).extend(button.button_schema(
    TopTronicButton
)).extend(CONFIG_SCHEMA_BASE)


async def to_code(config):
    tt = await cg.get_variable(config[CONF_TT_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await button.register_button(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(var.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(var.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_value(config[CONF_VALUE]))

    cg.add(tt.add_input(var))
