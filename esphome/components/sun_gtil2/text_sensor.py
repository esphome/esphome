import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATE

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


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SUN_GTIL2_ID])
    if state_config := config.get(CONF_STATE):
        sens = await text_sensor.new_text_sensor(state_config)
        cg.add(hub.set_state(sens))
    if serial_number_config := config.get(CONF_SERIAL_NUMBER):
        sens = await text_sensor.new_text_sensor(serial_number_config)
        cg.add(hub.set_serial_number(sens))
