import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ICON, ICON_KEY_PLUS

from . import CONF_HLK_FM22X_ID, HlkFm22xComponent

DEPENDENCIES = ["hlk_fm22x"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_ICON, default=ICON_KEY_PLUS): cv.icon,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(hub.set_enrolling_binary_sensor(var))
