import logging
from typing import Any

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import ICON_KEY_PLUS
from esphome.types import ConfigType

from . import CONF_HLK_FM22X_ID, ICON_FACE_RECOGNITION, HlkFm22xComponent

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["hlk_fm22x"]

CONF_ENROLLING = "enrolling"
CONF_SCANNING = "scanning"

_KEYED_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_ENROLLING): binary_sensor.binary_sensor_schema(
            icon=ICON_KEY_PLUS
        ),
        cv.Optional(CONF_SCANNING): binary_sensor.binary_sensor_schema(
            icon=ICON_FACE_RECOGNITION
        ),
    }
)


def _move_flat_options_under_enrolling(config: Any) -> Any:
    # Older configs describe the enrolling sensor directly under the platform entry.
    # Remove before 2027.5.0
    if not isinstance(config, dict) or not (
        flat := {
            key: value
            for key, value in config.items()
            if key not in (CONF_HLK_FM22X_ID, CONF_ENROLLING, CONF_SCANNING)
        }
    ):
        return config
    _LOGGER.warning(
        "[hlk_fm22x] Configuring the enrolling binary sensor directly under the "
        "platform is deprecated, move its options under 'enrolling:'. "
        "Will be removed in 2027.5.0"
    )
    moved = {key: value for key, value in config.items() if key not in flat}
    moved[CONF_ENROLLING] = flat
    return moved


CONFIG_SCHEMA = cv.All(_move_flat_options_under_enrolling, _KEYED_SCHEMA)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_ENROLLING, hub.set_enrolling_binary_sensor)
    await binary_sensors(CONF_SCANNING, hub.set_scanning_binary_sensor)
