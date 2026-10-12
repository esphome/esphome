import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP, ICON_FINGERPRINT
from esphome.types import ConfigType

from . import CONF_UFM01_ID, UFM01Component

DEPENDENCIES = ["ufm01"]

CONF_METER_ID = "meter_id"
CONF_SOFTWARE_VERSION = "software_version"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_UFM01_ID): cv.use_id(UFM01Component),
    cv.Optional(CONF_METER_ID): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_FINGERPRINT
    ),
    cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_UFM01_ID])
    sensors = text_sensor.sub_text_sensors(config)
    if await sensors(CONF_METER_ID, hub.set_meter_id_text_sensor) is not None:
        cg.add_define("USE_UFM01_METER_ID")
    if (
        await sensors(CONF_SOFTWARE_VERSION, hub.set_software_version_text_sensor)
        is not None
    ):
        cg.add_define("USE_UFM01_SOFTWARE_VERSION")
