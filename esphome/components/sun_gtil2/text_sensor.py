import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_STATE
from . import SunGTIL2Component, CONF_SUN_GTIL2_ID

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
    if CONF_STATE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATE])
        cg.add(hub.set_state(sens))
    if CONF_SERIAL_NUMBER in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SERIAL_NUMBER])
        cg.add(hub.set_serial_number(sens))
