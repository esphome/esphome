import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import CONF_OUTLAW_990_ID, Outlaw990

DEPENDENCIES = ["outlaw_990"]

CONF_SENSOR_POWER = "sensor_power"
CONF_SENSOR_MUTE = "sensor_mute"

TYPES = [
    CONF_SENSOR_POWER,
    CONF_SENSOR_MUTE,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OUTLAW_990_ID): cv.use_id(Outlaw990),
        cv.Required(CONF_SENSOR_POWER): binary_sensor.binary_sensor_schema(),
        cv.Required(CONF_SENSOR_MUTE): binary_sensor.binary_sensor_schema(),
    }
)


async def to_code(config):
    comp = await cg.get_variable(config[CONF_OUTLAW_990_ID])

    if CONF_SENSOR_POWER in config:
        conf = config[CONF_SENSOR_POWER]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(comp.set_power_binary_sensor(sens))

    if CONF_SENSOR_MUTE in config:
        conf = config[CONF_SENSOR_MUTE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(comp.set_mute_binary_sensor(sens))
