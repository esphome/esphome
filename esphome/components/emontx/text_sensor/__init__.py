import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP
from esphome.types import ConfigType

from .. import CONF_EMONTX_ID, EmonTx

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_EMONTX_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    if await text_sensors(CONF_VERSION, hub.set_version_text_sensor) is not None:
        cg.add_define("USE_EMONTX_VERSION")
        cg.add_define("USE_EMONTX_VERSION_TEXT_SENSOR")
