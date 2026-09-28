import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_PROJECT
from esphome.types import ConfigType

from . import CONF_LD6004_ID, LD6004Component

CONF_FIRMWARE_VERSION: str = "firmware_version"
CONF_WORK_STATUS: str = "work_status"
CONF_COMMAND_STATUS: str = "command_status"

DEPENDENCIES: list[str] = ["ld6004"]

FIELDS: tuple[str, ...] = (
    CONF_PROJECT,
    CONF_FIRMWARE_VERSION,
    CONF_WORK_STATUS,
    CONF_COMMAND_STATUS,
)
CONFIG_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.GenerateID(CONF_LD6004_ID): cv.use_id(LD6004Component),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(
                entity_category="diagnostic"
            )
            for key in FIELDS
        },
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD6004_TEXT_SENSOR")
    conf: ConfigType | None
    hub: cg.MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    for index, key in enumerate(FIELDS):
        if conf := config.get(key):
            entity: cg.MockObj = await text_sensor.new_text_sensor(conf)
            cg.add(hub.set_text_sensor(index, entity))
