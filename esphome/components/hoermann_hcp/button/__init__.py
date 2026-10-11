import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ICON_AIR_FILTER
from esphome.types import ConfigType

from .. import CONF_HOERMANN_HCP_ID, HoermannHcp, hoermann_hcp_ns

DEPENDENCIES = ["hoermann_hcp"]

CONF_HALF_OPEN = "half_open"
CONF_VENT = "vent"

ICON_GARAGE_OPEN_VARIANT = "mdi:garage-open-variant"

HoermannHcpVentButton = hoermann_hcp_ns.class_("HoermannHcpVentButton", button.Button)
HoermannHcpHalfOpenButton = hoermann_hcp_ns.class_(
    "HoermannHcpHalfOpenButton", button.Button
)

BUTTON_KEYS = (CONF_VENT, CONF_HALF_OPEN)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HOERMANN_HCP_ID): cv.use_id(HoermannHcp),
            cv.Optional(CONF_VENT): button.button_schema(
                HoermannHcpVentButton, icon=ICON_AIR_FILTER
            ),
            cv.Optional(CONF_HALF_OPEN): button.button_schema(
                HoermannHcpHalfOpenButton, icon=ICON_GARAGE_OPEN_VARIANT
            ),
        }
    ),
    cv.has_at_least_one_key(*BUTTON_KEYS),
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HOERMANN_HCP_ID])
    for key in BUTTON_KEYS:
        if (conf := config.get(key)) is not None:
            await button.new_button(conf, parent)
