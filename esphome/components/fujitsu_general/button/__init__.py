import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from .. import fujitsu_general_ns
from ..climate import FujitsuGeneralClimate

CONF_FUJITSU_GENERAL_ID = "fujitsu_general_id"
CONF_STEP_VERTICAL = "step_vertical"
CONF_STEP_HORIZONTAL = "step_horizontal"
CONF_ECONOMY = "economy"

FujitsuGeneralButton = fujitsu_general_ns.class_(
    "FujitsuGeneralButton", button.Button, cg.Component
)

BUTTON_CONFIGS = (
    (CONF_STEP_VERTICAL, "Step Vertical Vane", 0x6C, "mdi:arrow-up-down-bold"),
    (CONF_STEP_HORIZONTAL, "Step Horizontal Vane", 0x79, "mdi:arrow-left-right-bold"),
    (CONF_ECONOMY, "Economy Toggle", 0x09, "mdi:leaf"),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FUJITSU_GENERAL_ID): cv.use_id(FujitsuGeneralClimate),
        **{
            cv.Optional(key): button.button_schema(FujitsuGeneralButton, icon=icon)
            for key, _, _, icon in BUTTON_CONFIGS
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FUJITSU_GENERAL_ID])
    for key, default_name, command_byte, _ in BUTTON_CONFIGS:
        if button_conf := config.get(key):
            var = cg.new_Pvariable(button_conf[cv.CONF_ID], default_name, command_byte)
            await button.register_button(var, button_conf)
            await cg.register_component(var, button_conf)
            await cg.register_parented(var, parent)
