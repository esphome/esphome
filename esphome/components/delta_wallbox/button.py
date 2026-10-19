import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_DELTA_WALLBOX_ID, DeltaWallbox, delta_wallbox_ns

DEPENDENCIES = ["delta_wallbox"]

CONF_START_CHARGING = "start_charging"
CONF_STOP_CHARGING = "stop_charging"

DeltaWallboxStartChargingButton = delta_wallbox_ns.class_(
    "DeltaWallboxStartChargingButton", button.Button
)
DeltaWallboxStopChargingButton = delta_wallbox_ns.class_(
    "DeltaWallboxStopChargingButton", button.Button
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DELTA_WALLBOX_ID): cv.use_id(DeltaWallbox),
        cv.Optional(CONF_START_CHARGING): button.button_schema(
            DeltaWallboxStartChargingButton,
            icon="mdi:play-circle-outline",
        ),
        cv.Optional(CONF_STOP_CHARGING): button.button_schema(
            DeltaWallboxStopChargingButton,
            icon="mdi:stop-circle-outline",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_DELTA_WALLBOX_ID])

    if start_button_config := config.get(CONF_START_CHARGING):
        button_var = await button.new_button(start_button_config)
        await cg.register_parented(button_var, parent)

    if stop_button_config := config.get(CONF_STOP_CHARGING):
        button_var = await button.new_button(stop_button_config)
        await cg.register_parented(button_var, parent)
