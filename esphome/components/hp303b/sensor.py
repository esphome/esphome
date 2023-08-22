import esphome.config_validation as cv
from esphome.components import sensor
from . import hp303b_ns, HP303BComponent, CONF_HP303B_ID

DEPENDENCIES = ["hp303b"]

HP303BSensor = hp303b_ns.class_("HP303BSensor", sensor.Sensor)


CONFIG_SCHEMA = sensor.sensor_schema(HP303BSensor).extend(
    {
        cv.GenerateID(CONF_HP303B_ID): cv.use_id(HP303BComponent),
    }
)


async def to_code(config):
    _ = await sensor.new_sensor(config)
