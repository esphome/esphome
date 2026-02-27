import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from .. import fujitsu_general_ns
from ..climate import FujitsuGeneralClimate

CONF_FUJITSU_GENERAL_ID = "fujitsu_general_id"
CONF_STEP_VERTICAL = "step_vertical"
CONF_STEP_HORIZONTAL = "step_horizontal"
CONF_ECONOMY = "economy"

FujitsuGeneralButton = fujitsu_general_ns.class_("FujitsuGeneralButton", button.Button)

BUTTON_CONFIGS = (
    (
        CONF_STEP_VERTICAL,
        "Step Vertical Vane",
        "FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_VERTICAL",
        "mdi:arrow-up-down-bold",
    ),
    (
        CONF_STEP_HORIZONTAL,
        "Step Horizontal Vane",
        "FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_HORIZONTAL",
        "mdi:arrow-left-right-bold",
    ),
    (
        CONF_ECONOMY,
        "Economy Toggle",
        "FUJITSU_GENERAL_MESSAGE_TYPE_ECONOMY",
        "mdi:leaf",
    ),
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
    for key, default_name, command, _ in BUTTON_CONFIGS:
        if button_conf := config.get(key):
            var = cg.new_Pvariable(
                button_conf[cv.CONF_ID], getattr(fujitsu_general_ns, command)
            )
            await button.register_button(var, button_conf)
            await cg.register_parented(var, parent)
