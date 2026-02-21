import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP

from . import CONF_GDK101_ID, GDK101Component

DEPENDENCIES = ["gdk101"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GDK101_ID): cv.use_id(GDK101Component),
        cv.Required(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_GDK101_ID])
    var = await text_sensor.new_text_sensor(config[CONF_VERSION])
    cg.add(hub.set_fw_version_text_sensor(var))
