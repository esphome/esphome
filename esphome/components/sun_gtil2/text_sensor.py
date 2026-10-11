import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATE
from esphome.types import ConfigType

from . import CONF_SUN_GTIL2_ID, SunGTIL2Component

CONF_SERIAL_NUMBER = "serial_number"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_SUN_GTIL2_ID): cv.use_id(SunGTIL2Component),
            cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
                text_sensor.TextSensor
            ),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
                text_sensor.TextSensor
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SUN_GTIL2_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_STATE, hub.set_state)
    await text_sensors(CONF_SERIAL_NUMBER, hub.set_serial_number)
