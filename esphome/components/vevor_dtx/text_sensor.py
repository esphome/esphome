import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_SENSOR_ID
from esphome.types import ConfigType

from . import CONF_VEVOR_DTX_ID, VevorDtxComponent

DEPENDENCIES = ["vevor_dtx"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VEVOR_DTX_ID): cv.use_id(VevorDtxComponent),
        cv.Optional(CONF_SENSOR_ID): text_sensor.text_sensor_schema(
            icon="mdi:identifier",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_VEVOR_DTX_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_SENSOR_ID, hub.set_sensor_id_text_sensor)
