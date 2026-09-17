import logging
from typing import Any

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ICON, CONF_ID, CONF_NAME, ICON_KEY_PLUS
from esphome.types import ConfigType

from . import CONF_HLK_FM22X_ID, ICON_FACE_RECOGNITION, HlkFm22xComponent

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["hlk_fm22x"]

CONF_ENROLLING = "enrolling"
CONF_SCANNING = "scanning"

SENSOR_KEYS = (CONF_ENROLLING, CONF_SCANNING)

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

# Older configs describe the enrolling sensor directly under the platform entry.
# Remove before 2027.3.0
_LEGACY_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_ICON, default=ICON_KEY_PLUS): cv.icon,
    }
)


def _validate(config: Any) -> ConfigType:
    # An entity name or id at the top level can only come from the old single sensor form
    if isinstance(config, dict) and (CONF_NAME in config or CONF_ID in config):
        # Remove before 2027.3.0
        _LOGGER.warning(
            "[hlk_fm22x] Configuring the enrolling binary sensor directly under the "
            "platform is deprecated, move its options under 'enrolling:'. "
            "Will be removed in 2027.3.0"
        )
        legacy = _LEGACY_SCHEMA(config)
        return {
            CONF_HLK_FM22X_ID: legacy.pop(CONF_HLK_FM22X_ID),
            CONF_ENROLLING: legacy,
        }
    return _KEYED_SCHEMA(config)


CONFIG_SCHEMA = _validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])
    for key in SENSOR_KEYS:
        if (conf := config.get(key)) is not None:
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(getattr(hub, f"set_{key}_binary_sensor")(sens))
