import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.core import TimePeriod
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_MT6701_SPI_ID, MT6701SPIComponent

DEPENDENCIES = ["mt6701_spi"]

CONF_PUSH_BUTTON = "push_button"
CONF_TRACK_LOSS = "track_loss"

# The button state is read with the angle, so a press shorter than the hub's interval is missed
MAX_PUSH_BUTTON_INTERVAL = TimePeriod(seconds=1)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_MT6701_SPI_ID): cv.use_id(MT6701SPIComponent),
    cv.Optional(CONF_PUSH_BUTTON): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_TRACK_LOSS): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


def _push_button_needs_a_fast_hub(config: ConfigType) -> ConfigType:
    if CONF_PUSH_BUTTON not in config:
        return config
    hub_id = config[CONF_MT6701_SPI_ID]
    for hub in fv.full_config.get()["mt6701_spi"]:
        if (
            hub[CONF_ID] == hub_id
            and hub[CONF_UPDATE_INTERVAL] > MAX_PUSH_BUTTON_INTERVAL
        ):
            raise cv.Invalid(
                f"push_button is sampled on the hub's update_interval; set it to "
                f"{MAX_PUSH_BUTTON_INTERVAL} or less on mt6701_spi '{hub_id}'"
            )
    return config


FINAL_VALIDATE_SCHEMA = _push_button_needs_a_fast_hub


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MT6701_SPI_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_PUSH_BUTTON, hub.set_push_button_binary_sensor)
    await binary_sensors(CONF_TRACK_LOSS, hub.set_track_loss_binary_sensor)
