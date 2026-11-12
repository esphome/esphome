import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ICON_BATTERY
from esphome.types import ConfigType

from . import CONF_NPM1300_ID, NPM1300_CHILD_SCHEMA

DEPENDENCIES = ["npm1300"]

CONF_CHARGER_STATUS = "charger_status"

CONFIG_SCHEMA = NPM1300_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_CHARGER_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_BATTERY,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_NPM1300_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_CHARGER_STATUS, hub.set_charger_status_text_sensor)
