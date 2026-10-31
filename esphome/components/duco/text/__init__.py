from esphome.components import text
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, DucoDevice, duco_ns, register_duco_device

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoNightboostTimeStart = duco_ns.class_("DucoNightboostTimeStart", DucoDevice, text.Text)
DucoNightboostTimeStop = duco_ns.class_("DucoNightboostTimeStop", DucoDevice, text.Text)

CONF_NIGHTBOOST_TIME_START = "nightboost_time_start"
CONF_NIGHTBOOST_TIME_STOP = "nightboost_time_stop"
TIME_PATTERN = r"^([01][0-9]|2[0-3]):[0-5][0-9]$"

NIGHTBOOST_TIME_START_SCHEMA = text.text_schema(DucoNightboostTimeStart, mode="TEXT").extend(
    cv.polling_component_schema("60s")
)

NIGHTBOOST_TIME_STOP_SCHEMA = text.text_schema(DucoNightboostTimeStop, mode="TEXT").extend(
    cv.polling_component_schema("60s")
)

CONFIG_SCHEMA = DUCO_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_NIGHTBOOST_TIME_START): NIGHTBOOST_TIME_START_SCHEMA,
        cv.Optional(CONF_NIGHTBOOST_TIME_STOP): NIGHTBOOST_TIME_STOP_SCHEMA,
    }
)


async def to_code(config: ConfigType) -> None:
    for key in (CONF_NIGHTBOOST_TIME_START, CONF_NIGHTBOOST_TIME_STOP):
        if entity_config := config.get(key):
            var = await text.new_text(entity_config, min_length=5, max_length=5, pattern=TIME_PATTERN)
            await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
