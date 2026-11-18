import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import ICON_LIGHTBULB
from esphome.types import ConfigType

from . import CONF_TINYUSB_KEYBOARD_ID, TinyUSBKeyboard

DEPENDENCIES = ["tinyusb_keyboard"]

CONF_CAPSLOCK = "capslock"
CONF_NUMLOCK = "numlock"
CONF_SCROLLLOCK = "scrolllock"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_TINYUSB_KEYBOARD_ID): cv.use_id(TinyUSBKeyboard),
    cv.Optional(CONF_CAPSLOCK): binary_sensor.binary_sensor_schema(icon=ICON_LIGHTBULB),
    cv.Optional(CONF_NUMLOCK): binary_sensor.binary_sensor_schema(icon=ICON_LIGHTBULB),
    cv.Optional(CONF_SCROLLLOCK): binary_sensor.binary_sensor_schema(
        icon=ICON_LIGHTBULB
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TINYUSB_KEYBOARD_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_CAPSLOCK, hub.set_capslock_binary_sensor)
    await binary_sensors(CONF_NUMLOCK, hub.set_numlock_binary_sensor)
    await binary_sensors(CONF_SCROLLLOCK, hub.set_scrolllock_binary_sensor)
