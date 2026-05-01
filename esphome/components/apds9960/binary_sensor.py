import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, DEVICE_CLASS_MOVING

from . import APDS9960, CONF_APDS9960_ID

DEPENDENCIES = ["apds9960"]

DIRECTIONS = ["up", "down", "left", "right"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_MOVING
).extend(
    {
        cv.GenerateID(CONF_APDS9960_ID): cv.use_id(APDS9960),
        cv.Required(CONF_DIRECTION): cv.one_of(*DIRECTIONS, lower=True),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_APDS9960_ID])
    var = await binary_sensor.new_binary_sensor(config)
    func = getattr(hub, f"set_{config[CONF_DIRECTION]}_direction_binary_sensor")
    cg.add(func(var))
