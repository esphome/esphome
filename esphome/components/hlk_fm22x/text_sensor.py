import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ICON,
    ICON_RESTART,
)
from . import CONF_HLK_FM22X_ID, HlkFm22xComponent

DEPENDENCIES = ["hlk_fm22x"]


CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_HLK_FM22X_ID): cv.use_id(HlkFm22xComponent),
        cv.Optional(CONF_ICON, default=ICON_RESTART): cv.icon,
    }
)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_HLK_FM22X_ID])
    sens = await text_sensor.new_text_sensor(config)
    cg.add(hub.set_version_text_sensor(sens))
