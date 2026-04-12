import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ICON, ICON_KEY_PLUS

from . import CONF_FINGERPRINT_GROW_ID, FingerprintGrowComponent

DEPENDENCIES = ["fingerprint_grow"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_FINGERPRINT_GROW_ID): cv.use_id(FingerprintGrowComponent),
        cv.Optional(CONF_ICON, default=ICON_KEY_PLUS): cv.icon,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_FINGERPRINT_GROW_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(hub.set_enrolling_binary_sensor(var))
