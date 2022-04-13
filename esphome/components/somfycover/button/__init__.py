import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, globals
from esphome.const import (
    CONF_ID,
)
CODEOWNERS = ["@icarome"]

somfyprogbutton_ns = cg.esphome_ns.namespace("somfyprogbutton")
SomfyProgButton = somfyprogbutton_ns.class_(
    "SomfyProgButton", button.Button, cg.Component
)
CONF_REMOTE_ID = "remote_id"
CONF_ROLLING_CODE = "rolling_code_id"

CONFIG_SCHEMA = (
    button.BUTTON_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SomfyProgButton),
            cv.Required(CONF_REMOTE_ID): cv.hex_uint32_t,
            cv.Required(CONF_ROLLING_CODE): cv.use_id(globals.GlobalsComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
    cg.add(var.set_remote_id(config[CONF_REMOTE_ID]))
    rolling_code_id = await cg.get_variable(config[CONF_ROLLING_CODE])
    cg.add(var.set_rolling_code_(rolling_code_id))
