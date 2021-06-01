import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_DIRECTION, CONF_DEVICE_CLASS, DEVICE_CLASS_MOVING
from . import APDS9960, CONF_APDS9960_ID

DEPENDENCIES = ["apds9960"]

DIRECTIONS = {
    "UP": "set_up_direction",
    "DOWN": "set_down_direction",
    "LEFT": "set_left_direction",
    "RIGHT": "set_right_direction",
}

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.Required(CONF_DIRECTION): cv.one_of(*DIRECTIONS, upper=True),
        cv.GenerateID(CONF_APDS9960_ID): cv.use_id(APDS9960),
        cv.Optional(
            CONF_DEVICE_CLASS, default=DEVICE_CLASS_MOVING
        ): binary_sensor.device_class,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_APDS9960_ID])
    var = await binary_sensor.new_binary_sensor(config)
    func = getattr(hub, DIRECTIONS[config[CONF_DIRECTION]])
    cg.add(func(var))
