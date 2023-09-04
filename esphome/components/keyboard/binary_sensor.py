import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import Keyboard

CONF_CAPSLOCK = "capslock"
CONF_NUMLOCK = "numlock"
CONF_SCROLLOCK = "scrollock"

TYPES = [
    CONF_CAPSLOCK,
    CONF_NUMLOCK,
    CONF_SCROLLOCK,
]

CONF_KEYBOARD_ID = "keyboard_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_KEYBOARD_ID): cv.use_id(Keyboard),
        cv.Optional(CONF_CAPSLOCK): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_NUMLOCK): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_SCROLLOCK): binary_sensor.binary_sensor_schema(),
    }
).extend(cv.COMPONENT_SCHEMA)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        var = await binary_sensor.new_binary_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_binary_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_KEYBOARD_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
