import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_AXP2101_ID, AXP2101Component

DEPENDENCIES = ["axp2101"]

CONF_BATTERY_STATUS = "battery_status"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP2101_ID): cv.use_id(AXP2101Component),
        cv.Optional(CONF_BATTERY_STATUS): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:battery-heart"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AXP2101_ID])

    if CONF_BATTERY_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BATTERY_STATUS])
        cg.add(parent.set_battery_status_sensor(sens))
