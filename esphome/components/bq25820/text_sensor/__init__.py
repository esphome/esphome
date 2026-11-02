import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ICON_THERMOMETER
from esphome.types import ConfigType

from .. import CONF_BQ25820_ID, BQ25820BaseSchema

DEPENDENCIES = ["bq25820"]

CONF_CHARGE_STATUS = "charge_status"
CONF_TEMP_STATUS = "temp_status"

ICON_BATTERY_CHARGING = "mdi:battery-charging"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHARGE_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_BATTERY_CHARGING,
        ),
        cv.Optional(CONF_TEMP_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_THERMOMETER,
        ),
    }
).extend(BQ25820BaseSchema)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_CHARGE_STATUS, hub.set_charge_status_text_sensor)
    await text_sensors(CONF_TEMP_STATUS, hub.set_temp_status_text_sensor)
