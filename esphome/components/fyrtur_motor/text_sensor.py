import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_STATUS
from . import FyrturMotorComponent, CONF_FYRTUR_MOTOR_ID

ICON_PULSE = "mdi:pulse"

TYPES = [
    CONF_STATUS,
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_FYRTUR_MOTOR_ID): cv.use_id(FyrturMotorComponent),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(icon=ICON_PULSE),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_FYRTUR_MOTOR_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
