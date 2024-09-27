import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import FyrturMotorComponent, CONF_FYRTUR_MOTOR_ID

CONF_MOVING = "moving"
CONF_FULLY_OPEN = "fully_open"
CONF_FULLY_CLOSED = "fully_closed"
CONF_PARTIALY_OPEN = "partialy_open"

TYPES = [CONF_MOVING, CONF_FULLY_OPEN, CONF_FULLY_CLOSED, CONF_PARTIALY_OPEN]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_FYRTUR_MOTOR_ID): cv.use_id(FyrturMotorComponent),
            cv.Optional(CONF_MOVING): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_FULLY_OPEN): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_FULLY_CLOSED): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_PARTIALY_OPEN): binary_sensor.binary_sensor_schema(),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        var = await binary_sensor.new_binary_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_binary_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_FYRTUR_MOTOR_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
