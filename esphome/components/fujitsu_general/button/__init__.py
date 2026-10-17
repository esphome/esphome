import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import fujitsu_general_ns
from ..climate import FujitsuGeneralClimate

CONF_FUJITSU_GENERAL_ID = "fujitsu_general_id"
CONF_STEP_VERTICAL = "step_vertical"
CONF_STEP_HORIZONTAL = "step_horizontal"
CONF_ECONOMY = "economy"

FujitsuGeneralButton = fujitsu_general_ns.class_("FujitsuGeneralButton", button.Button)

# Config key, the command byte the button sends, icon
BUTTONS = (
    (
        CONF_STEP_VERTICAL,
        fujitsu_general_ns.FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_VERTICAL,
        "mdi:arrow-up-down-bold",
    ),
    (
        CONF_STEP_HORIZONTAL,
        fujitsu_general_ns.FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_HORIZONTAL,
        "mdi:arrow-left-right-bold",
    ),
    (CONF_ECONOMY, fujitsu_general_ns.FUJITSU_GENERAL_MESSAGE_TYPE_ECONOMY, "mdi:leaf"),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FUJITSU_GENERAL_ID): cv.use_id(FujitsuGeneralClimate),
        **{
            cv.Optional(key): button.button_schema(FujitsuGeneralButton, icon=icon)
            for key, _, icon in BUTTONS
        },
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_FUJITSU_GENERAL_ID])
    for key, command, _ in BUTTONS:
        if (conf := config.get(key)) is None:
            continue
        var = cg.new_Pvariable(conf[CONF_ID], command)
        await button.register_button(var, conf)
        await cg.register_parented(var, parent)
